cpp-peglib
==========

[![Build Status](https://travis-ci.org/yhirose/cpp-peglib.svg?branch=master)](https://travis-ci.org/yhirose/cpp-peglib)
[![Bulid Status](https://ci.appveyor.com/api/projects/status/github/yhirose/cpp-peglib?branch=master&svg=true)](https://ci.appveyor.com/project/yhirose/cpp-peglib)

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience in a simple way. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://www.brynosaurus.com/pub/lang/peg.pdf). *cpp-peglib* also supports the following additional syntax for now:

  * `<` ... `>` (Token boundary operator)
  * `~` (Ignore operator)
  * `\x20` (Hex number char)
  * `$name<` ... `>` (Named capture operator)
  * `$name` (Backreference operator)
  * `%whitespace` (Automatic whitespace skipping)
  * `%word` (Word expression)
  * `$name(` ... `)` (Capture scope operator)
  * `$name<` ... `>` (Named capture operator)
  * `$name` (Backreference operator)
  * `MACRO_NAME(` ... `)` (Parameterized rule or Macro)

This library also supports the linear-time parsing known as the [*Packrat*](http://pdos.csail.mit.edu/~baford/packrat/thesis/thesis.pdf) parsing.

If you need a Go language version, please see [*go-peg*](https://github.com/yhirose/go-peg).

How to use
----------

This is a simple calculator sample. It shows how to define grammar, associate samantic actions to the grammar, and handle semantic values.

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

    // (3) Setup actions
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

There are two semantic actions available:

```cpp
[](const SemanticValues& sv, any& dt)
[](const SemanticValues& sv)
```

`const SemanticValues& sv` contains the following information:

 - Semantic values
 - Matched string information
 - Token information if the rule is literal or uses a token boundary operator
 - Choice number when the rule is 'prioritized choise'

`any& dt` is a 'read-write' context data which can be used for whatever purposes. The initial context data is set in `peg::parser::parse` method.

`peg::any` is a simpler implementatin of [boost::any](http://www.boost.org/doc/libs/1_57_0/doc/html/any.html). It can wrap arbitrary data type.

A semantic action can return a value of arbitrary data type, which will be wrapped by `peg::any`. If a user returns nothing in a semantic action, the first semantic value in the `const SemanticValues& sv` argument will be returned. (Yacc parser has the same behavior.)

Here shows the `SemanticValues` structure:

```cpp
struct SemanticValues : protected std::vector<any>
{
    // Input text
    const char* path;
    const char* ss;

    // Matched string
    std::string str() const;    // Matched string
    const char* c_str() const;  // Matched string start
    size_t      length() const; // Matched string length

    // Line number and column at which the matched string is
    std::pair<size_t, size_t> line_info() const;

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

The following example uses `<` ... ` >` operator, which is *token boundary* operator.

```cpp
auto syntax = R"(
    ROOT  <- _ TOKEN (',' _ TOKEN)*
    TOKEN <- < [a-z0-9]+ > _
    _     <- [ \t\r\n]*
)";

peg pg(syntax);

pg["TOKEN"] = [](const auto& sv) {
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

parser["ROOT"] = [&](const auto& sv) {
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

parser["NUMBER"] = [](const auto& sv) {
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

parser["RULE"] = [](const auto& sv, any& dt) {
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

Word expression
---------------

```cpp
peg::parser parser(R"(
    ROOT         <-  'hello' 'world'
    %whitespace  <-  [ \t\r\n]*
    %word        <-  [a-z]+
)");

parser.parse("hello world"); // OK
parser.parse("helloworld");  // NG
```

Capture/Backreference
---------------------

```cpp
peg::parser parser(R"(
    ROOT      <- CONTENT
    CONTENT   <- (ELEMENT / TEXT)*
    ELEMENT   <- $(STAG CONTENT ETAG)
    STAG      <- '<' $tag< TAG_NAME > '>'
    ETAG      <- '</' $tag '>'
    TAG_NAME  <- 'b' / 'u'
    TEXT      <- TEXT_DATA
    TEXT_DATA <- ![<] .
)");

parser.parse("This is <b>a <u>test</u> text</b>."); // OK
parser.parse("This is <b>a <u>test</b> text</u>."); // NG
parser.parse("This is <b>a <u>test text</b>.");     // NG
```

Parameterized Rule or Macro
---------------------------

```peg
# Syntax
Start      ← _ Expr
Expr       ← Sum
Sum        ← List(Product, SumOpe)
Product    ← List(Value, ProOpe)
Value      ← Number / T('(') Expr T(')')

# Token
SumOpe     ← T('+' / '-')
ProOpe     ← T('*' / '/')
Number     ← T([0-9]+)
~_         ← [ \t\r\n]*

# Macro
List(I, D) ← I (D I)*
T(x)       ← < x > _
```

AST generation
--------------

*cpp-peglib* is able to generate an AST (Abstract Syntax Tree) when parsing. `enable_ast` method on `peg::parser` class enables the feature.

```
peg::parser parser("...");

parser.enable_ast();

shared_ptr<peg::Ast> ast;
if (parser.parse("...", ast)) {
    cout << peg::ast_to_s(ast);

    ast = peg::AstOptimizer(true).optimize(ast);
    cout << peg::ast_to_s(ast);
}
```

`peg::AstOptimizer` removes redundant nodes to make a AST simpler. You can make your own AST optimizers to fit your needs.

See actual usages in the [AST calculator example](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc) and [PL/0 language example](https://github.com/yhirose/cpp-peglib/blob/master/pl0/pl0.cc).

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
| csc      | Capture scope         |
| cap      | Capture               |
| bkr      | Back reference        |

Unicode support
---------------

Since cpp-peglib only accepts 8 bits characters, it probably accepts UTF-8 text. But `.` matches only a byte, not a Unicode character. Also, it dosn't support `\u????`.

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator (with parser operators)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [Calculator (AST version)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/peglint.cc)
  * [PL/0 language example](https://github.com/yhirose/cpp-peglib/blob/master/pl0/pl0.cc)
  * [A tiny PL/0 JIT compiler in less than 700 LOC with LLVM and PEG parser](https://github.com/yhirose/pl0-jit-compiler)

Tested compilers
----------------

  * Visual Studio 2017
  * Visual Studio 2015
  * Visual Studio 2013 with update 5
  * Clang++ 5.0.1
  * Clang++ 5.0
  * Clang++ 4.0
  * Clang++ 3.5
  * G++ 5.4 on Ubuntu 16.04

  IMPORTANT NOTE for Ubuntu: Need `-pthread` option when linking. See [#23](https://github.com/yhirose/cpp-peglib/issues/23#issuecomment-261126127).

TODO
----

  * Unicode support (`.` matches a Unicode char. `\u????`, `\p{L}`)

License
-------

MIT license (© 2018 Yuji Hirose)
