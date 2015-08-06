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
    parser["Additive"] = [](const SemanticValues& sv) {
        switch (sv.choice) {
        case 0:  // "Multitive '+' Additive"
            return sv[0].get<int>() + sv[1].get<int>();
        default: // "Multitive"
            return sv[0].get<int>();
        }
    };

    parser["Multitive"] = [](const SemanticValues& sv) {
        switch (sv.choice) {
        case 0:  // "Primary '*' Multitive"
            return sv[0].get<int>() * sv[1].get<int>();
        default: // "Primary"
            return sv[0].get<int>();
        }
    };

    parser["Number"] = [](const SemanticValues& sv) {
        return stoi(sv.str(), nullptr, 10);
    };

    // (4) Parse
    parser.packrat_parsing(); // Enable packrat parsing.

    int val;
    parser.parse("(1+2)*3", val);

    assert(val == 9);
}
```

Here are available actions:

```c++
[](const SemanticValues& sv, any& dt)
[](const SemanticValues& sv)
```

`const SemanticValues& sv` contains semantic values. `SemanticValues` structure is defined as follows.

```c++
struct SemanticValue {
    any         val;  // Semantic value
    const char* name; // Definition name for the sematic value
    const char* s;    // Token start for the semantic value
    size_t      n;    // Token length for the semantic value

    // Cast semantic value
    template <typename T> T& get();
    template <typename T> const T& get() const;

    // Get token
    std::string str() const;
};

struct SemanticValues : protected std::vector<SemanticValue>
{
    const char* s;      // Token start
    size_t      n;      // Token length
    size_t      choice; // Choice number (0 based index)

    // Get token
    std::string str() const;

    // Transform the semantice value vector to another vector
    template <typename T> vector<T> transform(size_t beg = 0, size_t end = -1) const;
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

pg["TOKEN"] = [](const SemanticValues& sv) {
    // 'token' doesn't include trailing whitespaces
    auto token = sv.str();
};

auto ret = pg.parse(" token1, token2 ");
```

We can ignore unnecessary semantic values from the list by using `~` operator.

```c++
peglib::peg parser(
    "  ROOT  <-  _ ITEM (',' _ ITEM _)*  "
    "  ITEM  <-  ([a-z])+                "
    "  ~_    <-  [ \t]*                  "
);

parser["ROOT"] = [&](const SemanticValues& sv) {
    assert(sv.size() == 2); // should be 2 instead of 5.
};

auto ret = parser.parse(" item1, item2 ");
```

The following grammar is same as the above.

```c++
peglib::peg parser(
    "  ROOT  <-  ~_ ITEM (',' ~_ ITEM ~_)*  "
    "  ITEM  <-  ([a-z])+                   "
    "  _     <-  [ \t]*                     "
);
```

*Semantic predicate* support is available. We can do it by throwing a `peglib::parse_error` exception in a semantic action.

```c++
peglib::peg parser("NUMBER  <-  [0-9]+");

parser["NUMBER"] = [](const SemanticValues& sv) {
    auto val = stol(sv.str(), nullptr, 10);
    if (val != 100) {
        throw peglib::parse_error("value error!!");
    }
    return val;
};

long val;
auto ret = parser.parse("100", val);
assert(ret == true);
assert(val == 100);

ret = parser.parse("200", val);
assert(ret == false);
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

It also supports named capture with the `$name<` ... `>` operator.

```c++
peglib::match m;

auto ret = peglib::peg_match(
    R"(
        ROOT      <-  _ ('[' $test< TAG_NAME > ']' _)*
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )",
    " [tag1] [tag:2] [tag-3] ",
    m);

auto cap = m.named_capture("test");

REQUIRE(ret == true);
REQUIRE(m.size() == 4);
REQUIRE(cap.size() == 3);
REQUIRE(m.str(cap[2]) == "tag-3");
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
TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const SemanticValues& sv) {
                tags.push_back(sv.str());
            };
_        <= zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

The following are available operators:

| Operator |     Description       |
| :------- | :-------------------- |
| seq      | Sequence              |
| cho      | Prioritized Choice    |
| zom      | Zero or More          |
| oom      | One or More           |
| opt      | Optional              |
| apd      | And predicate         |
| npd      | Not predicate         |
| lit      | Literal string        |
| cls      | Character class       |
| chr      | Character             |
| dot      | Any character         |
| anc      | Anchor character      |
| ign      | Ignore semantic value |
| cap      | Capture character     |
| usr      | User defiend parser   |

Adjust definitions
------------------

It's possible to add/override definitions.

```c++
auto syntax = R"(
    ROOT <- _ 'Hello' _ NAME '!' _
)";

Rules additional_rules = {
    {
        "NAME", usr([](const char* s, size_t n, SemanticValues& sv, any& c) -> size_t {
            static vector<string> names = { "PEG", "BNF" };
            for (const auto& name: names) {
                if (name.size() <= n && !name.compare(0, name.size(), s, name.size())) {
                    return name.size(); // processed length
                }
            }
            return -1; // parse error
        })
    },
    {
        "~_", zom(cls(" \t\r\n"))
    }
};

peg g = peg(syntax, additional_rules);

assert(g.parse(" Hello BNF! "));
```

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator (with parser operators)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [Calculator (AST version)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/cmdline/peglint.cc)
  * [PL/0 Interpreter](https://github.com/yhirose/cpp-peglib/blob/master/language/pl0/pl0.cc)

Tested Compilers
----------------

  * Visual Studio 2015
  * Clang 3.5

TODO
----

  * Unicode support

License
-------

MIT license (© 2015 Yuji Hirose)
