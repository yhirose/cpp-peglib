cpp-peglib
==========

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience in a simple way. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://www.brynosaurus.com/pub/lang/peg.pdf). *cpp-peglib* also supports the following additional syntax for now:

  * `<` ... `>` (Token boundary operator)
  * `~` (Ignore operator)
  * `\x20` (Hex number char)
  * `$<` ... `>` (Capture operator)
  * `$name<` ... `>` (Named capture operator)

This library also supports the linear-time parsing known as the [*Packrat*](http://pdos.csail.mit.edu/~baford/packrat/thesis/thesis.pdf) parsing.

How to use
----------

This is a simple calculator sample. It shows how to define grammar, associate samantic actions to the grammar and handle semantic values.

```cpp
// (1) Include the header file
#include <peglib.h>
#include <assert.h>

using namespace peg;
using namespace std;

int main(void) {
    // (2) Make a parser
    auto syntax = R"(
        # Grammar for Calculator...
        Additive    <- Multitive '+' Additive / Multitive
        Multitive   <- Primary '*' Multitive / Primary
        Primary     <- '(' Additive ')' / Number
        Number      <- < [0-9]+ >
        %whitespace <- [ \t]*
    )";

    parser parser(syntax);

    // (3) Setup an action
    parser["Additive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:  // "Multitive '+' Additive"
            return sv[0].get<int>() + sv[1].get<int>();
        default: // "Multitive"
            return sv[0].get<int>();
        }
    };

    parser["Multitive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:  // "Primary '*' Multitive"
            return sv[0].get<int>() * sv[1].get<int>();
        default: // "Primary"
            return sv[0].get<int>();
        }
    };

    parser["Number"] = [](const SemanticValues& sv) {
        return stoi(sv.token(), nullptr, 10);
    };

    // (4) Parse
    parser.enable_packrat_parsing(); // Enable packrat parsing.

    int val;
    parser.parse(" (1 + 2) * 3 ", val);

    assert(val == 9);
}
```

Here are available actions:

```cpp
[](const SemanticValues& sv, any& dt)
[](const SemanticValues& sv)
```

`const SemanticValues& sv` contains semantic values. `SemanticValues` structure is defined as follows.

```cpp
struct SemanticValues : protected std::vector<any>
{
    // Matched string
    std::string str() const;    // Matched string
    const char* c_str() const;  // Matched string start
    size_t      length() const; // Matched string length

    // Tokens
    std::vector<
        std::pair<
            const char*, // Token start
            size_t>>     // Token length
        tokens;

    std::string token(size_t id = 0) const;

    // Choice number (0 based index)
    size_t      choice() const;

    // Transform the semantic value vector to another vector
    template <typename T> vector<T> transform(size_t beg = 0, size_t end = -1) const;
}
```

`peg::any` class is very similar to [boost::any](http://www.boost.org/doc/libs/1_57_0/doc/html/any.html). You can obtain a value by castning it to the actual type. In order to determine the actual type, you have to check the return value type of the child action for the semantic value.

`any& dt` is a data object which can be used by the user for whatever purposes.

The following example uses `<` ... ` >` operators. They are the *token boundary* operators.

```cpp
auto syntax = R"(
    ROOT  <- _ TOKEN (',' _ TOKEN)*
    TOKEN <- < [a-z0-9]+ > _
    _     <- [ \t\r\n]*
)";

peg pg(syntax);

pg["TOKEN"] = [](const SemanticValues& sv) {
    // 'token' doesn't include trailing whitespaces
    auto token = sv.token();
};

auto ret = pg.parse(" token1, token2 ");
```

We can ignore unnecessary semantic values from the list by using `~` operator.

```cpp
peg::pegparser parser(
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

```cpp
peg::parser parser(
    "  ROOT  <-  ~_ ITEM (',' ~_ ITEM ~_)*  "
    "  ITEM  <-  ([a-z])+                   "
    "  _     <-  [ \t]*                     "
);
```

*Semantic predicate* support is available. We can do it by throwing a `peg::parse_error` exception in a semantic action.

```cpp
peg::parser parser("NUMBER  <-  [0-9]+");

parser["NUMBER"] = [](const SemanticValues& sv) {
    auto val = stol(sv.str(), nullptr, 10);
    if (val != 100) {
        throw peg::parse_error("value error!!");
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

*enter* and *leave* actions are also avalable.

```cpp
parser["RULE"].enter = [](any& dt) {
    std::cout << "enter" << std::endl;
};

parser["RULE"] = [](const SemanticValues& sv, any& dt) {
    std::cout << "action!" << std::endl;
};

parser["RULE"].leave = [](any& dt) {
    std::cout << "leave" << std::endl;
};
```

Ignoring Whitespaces
--------------------

As you can see in the first example, we can ignore whitespaces between tokens automatically with `%whitespace` rule.

`%whitespace` rule can be applied to the following three conditions:

  * trailing spaces on tokens
  * leading spaces on text
  * trailing spaces on literal strings in rules

These are valid tokens:

```
KEYWORD  <- 'keyword'
WORD     <-  < [a-zA-Z0-9] [a-zA-Z0-9-_]* >    # token boundary operator is used.
IDNET    <-  < IDENT_START_CHAR IDENT_CHAR* >  # token boundary operator is used.
```

The following grammar accepts ` one, "two three", four `.

```
ROOT         <- ITEM (',' ITEM)*
ITEM         <- WORD / PHRASE
WORD         <- < [a-z]+ >
PHRASE       <- < '"' (!'"' .)* '"' >

%whitespace  <-  [ \t\r\n]*
```

Simple interface
----------------

*cpp-peglib* provides std::regex-like simple interface for trivial tasks.

`peg::peg_match` tries to capture strings in the `$< ... >` operator and store them into `peg::match` object.

```cpp
peg::match m;

auto ret = peg::peg_match(
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

```cpp
peg::match m;

auto ret = peg::peg_match(
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

```cpp
using namespace peg;

auto syntax = R"(
    ROOT <- '[' $< [a-z0-9]+ > ']'
)";

auto s = " [tag1] [tag2] [tag3] ";

// peg::peg_search
parser pg(syntax);
size_t pos = 0;
auto n = strlen(s);
match m;
while (peg_search(pg, s + pos, n - pos, m)) {
    cout << m.str()  << endl; // entire match
    cout << m.str(1) << endl; // submatch #1
    pos += m.length();
}

// peg::peg_token_iterator
peg_token_iterator it(syntax, s);
while (it != peg_token_iterator()) {
    cout << it->str()  << endl; // entire match
    cout << it->str(1) << endl; // submatch #1
    ++it;
}

// peg::peg_token_range
for (auto& m: peg_token_range(syntax, s)) {
    cout << m.str()  << endl; // entire match
    cout << m.str(1) << endl; // submatch #1
}
```

Make a parser with parser combinators
-------------------------------------

Instead of makeing a parser by parsing PEG syntax text, we can also construct a parser by hand with *parser combinatorss*. Here is an example:

```cpp
using namespace peg;
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
| tok      | Token boundary        |
| ign      | Ignore semantic value |
| cap      | Capture character     |
| usr      | User defined parser   |

Adjust definitions
------------------

It's possible to add/override definitions.

```cpp
auto syntax = R"(
    ROOT <- _ 'Hello' _ NAME '!' _
)";

Rules additional_rules = {
    {
        "NAME", usr([](const char* s, size_t n, SemanticValues& sv, any& dt) -> size_t {
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

auto g = parser(syntax, additional_rules);

assert(g.parse(" Hello BNF! "));
```

Unicode support
---------------

Since cpp-peglib only accepts 8 bits characters, it probably accepts UTF-8 text. But `.` matches only a byte, not a Unicode character. Also, it dosn't support `\u????`. 

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator (with parser operators)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [Calculator (AST version)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/cmdline/peglint.cc)
  * [PL/0 Interpreter](https://github.com/yhirose/cpp-peglib/blob/master/language/pl0/pl0.cc)

Tested compilers
----------------

  * Visual Studio 2015
  * Visual Studio 2013 with update 5
  * Clang 3.5

TODO
----

  * Unicode support (`.` matches a Unicode char. `\u????`, `\p{L}`)
  * Allow `←` and `ε`

License
-------

MIT license (© 2015 Yuji Hirose)
