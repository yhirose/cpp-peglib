cpp-peglib
==========

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience in a simple way. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://pdos.csail.mit.edu/papers/parsing:popl04.pdf). *cpp-peglib* also supports the following additional syntax for now:

  * `<` ... `>` (Anchor operator)
  * `$<` ... `>` (Capture operator)
  * `$name<` ... `>` (Named capture operator)
  * `~` (Ignore operator)
  * `\x20` (Hex number char)

This library also supports the linear-time parsing known as the [*Packrat*](http://pdos.csail.mit.edu/~baford/packrat/thesis/thesis.pdf) parsing.

How to use
----------

This is a simple calculator sample. It shows how to define grammar, associate samantic actions to the grammar and handle semantic values.

```c++
// (1) Include the header file
#include <peglib.h>
#include <assert.h>

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
        nullptr,                                        // Default action
        [](const SemanticValues& sv) {
            return sv[0].get<int>() + sv[1].get<int>(); // "Multitive '+' Additive"
        },
        [](const SemanticValues& sv) { return sv[0]; }  // "Multitive"
    };

    parser["Multitive"] = [](const SemanticValues& sv) {
        switch (sv.choice) {
        case 0:  // "Multitive '+' Additive"
            return sv[0].get<int>() * sv[1].get<int>();
        default: // "Multitive"
            return sv[0].get<int>();
        }
    };

    parser["Number"] = [](const char* s, size_t n) {
        return stoi(string(s, n), nullptr, 10);
    };

    // (4) Parse
    parser.packrat_parsing(true); // Enable packrat parsing.

    int val;
    parser.parse("(1+2)*3", val);

    assert(val == 9);
}
```

Here is a complete list of available actions:

```c++
[](const SemanticValues& sv, any& dt)
[](const SemanticValues& sv)
[](const char* s, size_t n)
[]()
```

`const SemanticValues& sv` contains semantic values. `SemanticValues` structure is defined as follows.

```c++
struct SemanticValue {
    peglib::any val;  // Semantic value
    std::string name; // Definition name for the sematic value
    const char* s;    // Token start for the semantic value
    size_t      n;    // Token length for the semantic value

    // Utility method
    template <typename T> T& get();
    template <typename T> const T& get() const;
    std::string str() const;
};

struct SemanticValues : protected std::vector<SemanticValue>
{
    const char* s;      // Token start
    size_t      n;      // Token length
    size_t      choice; // Choice number (0 based index)

    using std::vector<T>::size;
    using std::vector<T>::operator[];
    using std::vector<T>::begin;
    using std::vector<T>::end;
    // NOTE: There are more std::vector methods available...

    // Transform the semantice values vector to another vector
    template <typename F> auto map(size_t beg, size_t end, F f) const -> vector<typename std::remove_const<decltype(f(SemanticValue()))>::type>;
    template <typename F> auto map(F f) const -> vector<typename std::remove_const<decltype(f(SemanticValue()))>::type>;
    template <typename T> auto map(size_t beg = 0, size_t end = -1) const -> vector<T>;
}
```

`peglib::any` class is very similar to [boost::any](http://www.boost.org/doc/libs/1_57_0/doc/html/any.html). You can obtain a value by castning it to the actual type. In order to determine the actual type, you have to check the return value type of the child action for the semantic value.

`const char* s, size_t n` gives a pointer and length of the matched string. This is same as `sv.s` and `sv.n`.

`any& dt` is a data object which can be used by the user for whatever purposes.

The following example uses `<` ... ` >` operators. They are the *anchor* operators. Each anchor operator creates a semantic value that contains `const char*` of the position. It could be useful to eliminate unnecessary characters.

```c++
auto syntax = R"(
    ROOT  <- _ TOKEN (',' _ TOKEN)*
    TOKEN <- < [a-z0-9]+ > _
    _     <- [ \t\r\n]*
)";

peg pg(syntax);

pg["TOKEN"] = [](const char* s, size_t n) {
    // 'token' doesn't include trailing whitespaces
    auto token = string(s, n);
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

parser["ROOT"] = [&](const SemanticValues& sv) {
    assert(sv.size() == 2); // should be 2 instead of 5.
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
auto n = strlen(s);
match m;
while (peg_search(pg, s + pos, n - pos, m)) {
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
TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const char* s, size_t n) {
                tags.push_back(string(s, n));
            };
_        <= zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

The following are available operators:

| Operator |     Description     |
| :------- | :------------------ |
| seq      | Sequence            |
| cho      | Prioritized Choice  |
| zom      | Zero or More        |
| oom      | One or More         |
| opt      | Optional            |
| apd      | And predicate       |
| npd      | Not predicate       |
| lit      | Literal string      |
| cls      | Character class     |
| chr      | Character           |
| dot      | Any character       |
| anc      | Anchor character    |
| cap      | Capture character   |
| usr      | User defiend parser |

Predicate control
-----------------

  * TODO

Adjust definitions
------------------

It's possible to add and override definitions with parser operaters.

```c++
auto syntax = R"(
    ROOT <- _ 'Hello' _ NAME '!' _
)";

Rules rules = {
    {
        "NAME", usr([](const char* s, size_t n, SemanticValues& sv, any& c) {
            static vector<string> names = { "PEG", "BNF" };
            for (const auto& n: names) {
                if (n.size() <= n && !n.compare(0, n.size(), s, n.size())) {
                    return success(n.size());
                }
            }
            return fail(s);
        })
    },
    {
        "~_", zom(cls(" \t\r\n"))
    }
};

peg g = peg(syntax, rules);

assert(g.parse(" Hello BNF! "));
```

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator (with parser operators)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [Calculator (AST version)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/peglint.cc)

Tested Compilers
----------------

  * Visual Studio 2013
  * Clang 3.5

TODO
----

  * Optimization of grammars
  * Unicode support

License
-------

MIT license (© 2015 Yuji Hirose)
