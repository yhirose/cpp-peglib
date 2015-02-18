cpp-peglib
==========

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience in a simple way. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://pdos.csail.mit.edu/papers/parsing:popl04.pdf). *cpp-peglib* also supports the following additional syntax for now:

  * `<` ... `>` (Anchor operator)
  * `$<` ... `>` (Capture operator)
  * `~` (Ignore operator)

How to use
----------

This is a simple calculator sample. It shows how to define grammar, associate samantic actions to the grammar and handle semantic values.

```c++
#include <assert.h>

// (1) Include the header file
#include <peglib.h>

using namespace peglib;
using namespace std;

int main(void) {

    // (2) Make a parser
    auto syntax = R"(
        # Grammar for Calculator...
        Additive  <- Multitive '+' Additive / Multitive
        Multitive <- Primary '*' Multitive / Primary
        Primary   <- '(' Additive ')' / Number
        Number    <- [0-9]+
    )";

    peg parser(syntax);

    // (3) Setup an action
    parser["Additive"] = {
        nullptr,                                      // Default action
        [](const vector<any>& v) {
            return v[0].get<int>() + v[1].get<int>(); // 1st choice
        },
        [](const vector<any>& v) { return v[0]; }     // 2nd choice
    };

    parser["Multitive"] = {
        nullptr,                                      // Default action
        [](const vector<any>& v) {
            return v[0].get<int>() * v[1].get<int>(); // 1st choice
        },
        [](const vector<any>& v) { return v[0]; }     // 2nd choice
    };

    /* This action is not necessary.
    parser["Primary"] = [](const vector<any>& v) {
        return v[0];
    };
    */

    parser["Number"] = [](const char* s, size_t l) {
        return stoi(string(s, l), nullptr, 10);
    };

    // (4) Parse
    int val;
    parser.parse("(1+2)*3", val);

    assert(val == 9);
}
```

Here is a complete list of available actions:

```c++
[](const char* s, size_t l, const std::vector<peglib::any>& v, any& c)
[](const char* s, size_t l, const std::vector<peglib::any>& v)
[](const char* s, size_t l)
[](const std::vector<peglib::any>& v, any& c)
[](const std::vector<peglib::any>& v)
[]()
```

`const char* s, size_t l` gives a pointer and length of the matched string.

`const std::vector<peglib::any>& v` contains semantic values. `peglib::any` class is very similar to [boost::any](http://www.boost.org/doc/libs/1_57_0/doc/html/any.html). You can obtain a value by castning it to the actual type. In order to determine the actual type, you have to check the return value type of the child action for the semantic value.

`any& c` is a context data which can be used by the user for whatever purposes.

The following example uses `<` and ` >` operators. They are the *anchor* operators. Each anchor operator creates a semantic value that contains `const char*` of the position. It could be useful to eliminate unnecessary characters.

```c++
auto syntax = R"(
    ROOT  <- _ TOKEN (',' _ TOKEN)*
    TOKEN <- < [a-z0-9]+ > _
    _     <- [ \t\r\n]*
)";

peg pg(syntax);

pg["TOKEN"] = [](const char* s, size_t l, const vector<any>& v) {
    // 'token' doesn't include trailing whitespaces
    auto token = string(s, l);
};

auto ret = pg.parse(" token1, token2 ");
```

We can ignore unnecessary semantic values from the list by using `~` operator.

```c++
peglib::peg parser(
    "  ROOT  <-  _ ITEM (',' _ ITEM _)* "
    "  ITEM  <-  ([a-z])+  "
    "  ~_    <-  [ \t]*    "
);

parser["ROOT"] = [&](const vector<any>& v) {
    assert(v.size() == 2); // should be 2 instead of 5.
};

auto ret = parser.parse(" item1, item2 ");
```

Simple interface
----------------

*cpp-peglib* provides std::regex-like simple interface for trivial tasks.

`peglib::peg_match` tries to capture strings in the `$< ... >` operator and store them into `peglib::match` object.

```c++
peglib::match m;
auto ret = peglib::peg_match(
    R"(
        ROOT      <-  _ ('[' $< TAG_NAME > ']' _)*
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )",
    " [tag1] [tag:2] [tag-3] ",
    m);

assert(ret == true);
assert(m.size() == 4);
assert(m.str(1) == "tag1");
assert(m.str(2) == "tag:2");
assert(m.str(3) == "tag-3");
```

There are some ways to *search* a peg pattern in a document.

```c++
using namespace peglib;

auto syntax = R"(
ROOT <- '[' $< [a-z0-9]+ > ']'
)";

auto s = " [tag1] [tag2] [tag3] ";

// peglib::peg_search
peg pg(syntax);
size_t pos = 0;
auto l = strlen(s);
match m;
while (peg_search(pg, s + pos, l - pos, m)) {
  cout << m.str()  << endl; // entire match
  cout << m.str(1) << endl; // submatch #1
  pos += m.length();
}

// peglib::peg_token_iterator
peg_token_iterator it(syntax, s);
while (it != peg_token_iterator()) {
  cout << it->str()  << endl; // entire match
  cout << it->str(1) << endl; // submatch #1
  ++it;
}

// peglib::peg_token_range
for (auto& m: peg_token_range(syntax, s)) {
  cout << m.str()  << endl; // entire match
  cout << m.str(1) << endl; // submatch #1
}
```

Make a parser with parser operators
-----------------------------------

Instead of makeing a parser by parsing PEG syntax text, we can also construct a parser by hand with *parser operators*. Here is an example:

```c++
using namespace peglib;
using namespace std;

vector<string> tags;

Definition ROOT, TAG_NAME, _;
ROOT     <= seq(_, zom(seq(chr('['), TAG_NAME, chr(']'), _)));
TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const char* s, size_t l) {
                tags.push_back(string(s, l));
            };
_        <= zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

The following are available operators:

| Operator | Description        |
|:---------|:-------------------|
| seq      | Sequence           |
| cho      | Prioritized Choice |
| zom      | Zero or More       |
| oom      | One or More        |
| opt      | Optional           |
| apd      | And predicate      |
| npd      | Not predicate      |
| lit      | Literal string     |
| cls      | Character class    |
| chr      | Character          |
| dot      | Any character      |
| anc      | Anchor character   |
| cap      | Capture character  |

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator with parser operators](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/peglint.cc)

Tested Compilers
----------------

  * Visual Studio 2013
  * Clang 3.5

TODO
----

  * Linear-time parsing (Packrat parsing)
  * Optimization of grammars
  * Unicode support

Other C++ PEG parser libraries
------------------------------

Thanks to the authors of the libraries that inspired *cpp-peglib*.

 * [Boost Spirit X3](https://github.com/djowel/spirit_x3) - A set of C++ libraries for parsing and output generation implemented as Domain Specific Embedded Languages (DSEL) using Expression templates and Template Meta-Programming
 * [PEGTL](https://github.com/ColinH/PEGTL) - Parsing Expression Grammar Template Library
 * [lars::Parser](https://github.com/TheLartians/Parser) - A header-only linear-time c++ parsing expression grammar (PEG) parser generator supporting left-recursion and grammar ambiguity


License
-------

MIT license (© 2015 Yuji Hirose)
