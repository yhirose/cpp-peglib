cpp-peglib
==========

[![](https://github.com/yhirose/cpp-peglib/workflows/CMake/badge.svg)](https://github.com/yhirose/cpp-peglib/actions)

C++17 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library. You can start using it right away just by including `peglib.h` in your project.

Since this library only supports C++17 compilers, please make sure that the compiler option `-std=c++17` is enabled.
(`/std:c++17 /Zc:__cplusplus` for MSVC)

You can also try the online version, PEG Playground at https://yhirose.github.io/cpp-peglib.

The PEG syntax is well described on page 2 in the [document](http://www.brynosaurus.com/pub/lang/peg.pdf) by Bryan Ford. *cpp-peglib* also supports the following additional syntax for now:

* `'...'i` (Case-insensitive literal operator)
* `[...]i` (Case-insensitive character class operator)
* `[^...]` (Negated character class operator)
* `[^...]i` (Case-insensitive negated character class operator)
* `{2,5}` (Regex-like repetition operator)
* `<` ... `>` (Token boundary operator)
* `~` (Ignore operator)
* `\x20` (Hex number char)
* `\u10FFFF` (Unicode char)
* `%whitespace` (Automatic whitespace skipping)
* `%word` (Word expression)
* `$name(` ... `)` (Capture scope operator)
* `$name<` ... `>` (Named capture operator)
* `$name` (Backreference operator)
* `|` (Dictionary operator)
* `↑` (Cut operator)
* `MACRO_NAME(` ... `)` (Parameterized rule or Macro)
* `{ precedence L - + L / * }` (Parsing infix expression)
* `%recovery(` ... `)` (Error recovery operator)
* `exp⇑label` or `exp^label` (Syntax sugar for `(exp / %recover(label))`)
* `label { error_message "..." }` (Error message instruction)
* `{ no_ast_opt }` (No AST node optimization instruction)

'End of Input' check will be done as default. To disable the check, please call `disable_eoi_check`.

This library supports the linear-time parsing known as the [*Packrat*](http://pdos.csail.mit.edu/~baford/packrat/thesis/thesis.pdf) parsing.

IMPORTANT NOTE for some Linux distributions such as Ubuntu and CentOS: Need `-pthread` option when linking. See [#23](https://github.com/yhirose/cpp-peglib/issues/23#issuecomment-261126127), [#46](https://github.com/yhirose/cpp-peglib/issues/46#issuecomment-417870473) and [#62](https://github.com/yhirose/cpp-peglib/issues/62#issuecomment-492032680).

I am sure that you will enjoy this excellent ["Practical parsing with PEG and cpp-peglib"](https://berthub.eu/articles/posts/practical-peg-parsing/) article by [bert hubert](https://berthub.eu/)!

How to use
----------

This is a simple calculator sample. It shows how to define grammar, associate semantic actions to the grammar, and handle semantic values.

```cpp
// (1) Include the header file
#include <peglib.h>
#include <assert.h>
#include <iostream>

using namespace peg;
using namespace std;

int main(void) {
  // (2) Make a parser
  parser parser(R"(
    # Grammar for Calculator...
    Additive    <- Multiplicative '+' Additive / Multiplicative
    Multiplicative   <- Primary '*' Multiplicative / Primary
    Primary     <- '(' Additive ')' / Number
    Number      <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");

  assert(static_cast<bool>(parser) == true);

  // (3) Setup actions
  parser["Additive"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: // "Multiplicative '+' Additive"
      return any_cast<int>(vs[0]) + any_cast<int>(vs[1]);
    default: // "Multiplicative"
      return any_cast<int>(vs[0]);
    }
  };

  parser["Multiplicative"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: // "Primary '*' Multiplicative"
      return any_cast<int>(vs[0]) * any_cast<int>(vs[1]);
    default: // "Primary"
      return any_cast<int>(vs[0]);
    }
  };

  parser["Number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };

  // (4) Parse
  parser.enable_packrat_parsing(); // Enable packrat parsing.

  int val;
  parser.parse(" (1 + 2) * 3 ", val);

  assert(val == 9);
}
```

To show syntax errors in grammar text:

```cpp
auto grammar = R"(
  # Grammar for Calculator...
  Additive    <- Multiplicative '+' Additive / Multiplicative
  Multiplicative   <- Primary '*' Multiplicative / Primary
  Primary     <- '(' Additive ')' / Number
  Number      <- < [0-9]+ >
  %whitespace <- [ \t]*
)";

parser parser;

parser.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
  cerr << line << ":" << col << ": " << msg << "\n";
});

auto ok = parser.load_grammar(grammar);
assert(ok);
```

There are four semantic actions available:

```cpp
[](const SemanticValues& vs, any& dt)
[](const SemanticValues& vs)
[](SemanticValues& vs, any& dt)
[](SemanticValues& vs)
```

`SemanticValues` value contains the following information:

* Semantic values
* Matched string information
* Token information if the rule is literal or uses a token boundary operator
* Choice number when the rule is 'prioritized choice'

`any& dt` is a 'read-write' context data which can be used for whatever purposes. The initial context data is set in `peg::parser::parse` method.

A semantic action can return a value of arbitrary data type, which will be wrapped by `peg::any`. If a user returns nothing in a semantic action, the first semantic value in the `const SemanticValues& vs` argument will be returned. (Yacc parser has the same behavior.)

Here shows the `SemanticValues` structure:

```cpp
struct SemanticValues : protected std::vector<any>
{
  // Input text
  const char* path;
  const char* ss;

  // Matched string
  std::string_view sv() const { return sv_; }

  // Line number and column at which the matched string is
  std::pair<size_t, size_t> line_info() const;

  // Tokens
  std::vector<std::string_view> tokens;
  std::string_view token(size_t id = 0) const;

  // Token conversion
  std::string token_to_string(size_t id = 0) const;
  template <typename T> T token_to_number() const;

  // Choice number (0 based index)
  size_t choice() const;

  // Transform the semantic value vector to another vector
  template <typename T> vector<T> transform(size_t beg = 0, size_t end = -1) const;
}
```

The following example uses `<` ... `>` operator, which is *token boundary* operator.

```cpp
peg::parser parser(R"(
  ROOT  <- _ TOKEN (',' _ TOKEN)*
  TOKEN <- < [a-z0-9]+ > _
  _     <- [ \t\r\n]*
)");

parser["TOKEN"] = [](const SemanticValues& vs) {
  // 'token' doesn't include trailing whitespaces
  auto token = vs.token();
};

auto ret = parser.parse(" token1, token2 ");
```

We can ignore unnecessary semantic values from the list by using `~` operator.

```cpp
peg::parser parser(R"(
  ROOT  <-  _ ITEM (',' _ ITEM _)*
  ITEM  <-  ([a-z0-9])+
  ~_    <-  [ \t]*
)");

parser["ROOT"] = [&](const SemanticValues& vs) {
  assert(vs.size() == 2); // should be 2 instead of 5.
};

auto ret = parser.parse(" item1, item2 ");
```

The following grammar is the same as the above.

```cpp
peg::parser parser(R"(
  ROOT  <-  ~_ ITEM (',' ~_ ITEM ~_)*
  ITEM  <-  ([a-z0-9])+
  _     <-  [ \t]*
)");
```

*Semantic predicate* support is available with a *predicate* action.

```cpp
peg::parser parser("NUMBER  <-  [0-9]+");

parser["NUMBER"] = [](const SemanticValues &vs) {
  return vs.token_to_number<long>();
};

parser["NUMBER"].predicate = [](const SemanticValues &vs,
                                const std::any & /*dt*/, std::string &msg) {
  if (vs.token_to_number<long>() != 100) {
    msg = "value error!!";
    return false;
  }
  return true;
};

long val;
auto ret = parser.parse("100", val);
assert(ret == true);
assert(val == 100);

ret = parser.parse("200", val);
assert(ret == false);
```

*enter* and *leave* actions are also available.

```cpp
parser["RULE"].enter = [](const Context &c, const char* s, size_t n, any& dt) {
  std::cout << "enter" << std::endl;
};

parser["RULE"] = [](const SemanticValues& vs, any& dt) {
  std::cout << "action!" << std::endl;
};

parser["RULE"].leave = [](const Context &c, const char* s, size_t n, size_t matchlen, any& value, any& dt) {
  std::cout << "leave" << std::endl;
};
```

You can receive error information via a logger:

```cpp
parser.set_logger([](size_t line, size_t col, const string& msg) {
  ...
});

parser.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
  ...
});
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
KEYWORD   <- 'keyword'
KEYWORDI  <- 'case_insensitive_keyword'
WORD      <-  < [a-zA-Z0-9] [a-zA-Z0-9-_]* >    # token boundary operator is used.
IDNET     <-  < IDENT_START_CHAR IDENT_CHAR* >  # token boundary operator is used.
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

Dictionary
----------

`|` operator allows us to make a word dictionary for fast lookup by using Trie structure internally. We don't have to worry about the order of words.

```peg
START <- 'This month is ' MONTH '.'
MONTH <- 'Jan' | 'January' | 'Feb' | 'February' | '...'
```

We are able to find which item is matched with `choice()`.

```cpp
parser["MONTH"] = [](const SemanticValues &vs) {
  auto id = vs.choice();
};
```

It supports the case-insensitive mode.

```peg
START <- 'This month is ' MONTH '.'
MONTH <- 'Jan'i | 'January'i | 'Feb'i | 'February'i | '...'i
```

Cut operator
------------

`↑` operator could mitigate the backtrack performance problem, but has a risk to change the meaning of grammar.

```peg
S <- '(' ↑ P ')' / '"' ↑ P '"' / P
P <- 'a' / 'b' / 'c'
```

When we parse `(z` with the above grammar, we don't have to backtrack in `S` after `(` is matched, because a cut operator is inserted there.

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

Parsing infix expression by Precedence climbing
-----------------------------------------------

Regarding the *precedence climbing algorithm*, please see [this article](https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing).

```cpp
parser parser(R"(
  EXPRESSION             <-  INFIX_EXPRESSION(ATOM, OPERATOR)
  ATOM                   <-  NUMBER / '(' EXPRESSION ')'
  OPERATOR               <-  < [-+/*] >
  NUMBER                 <-  < '-'? [0-9]+ >
  %whitespace            <-  [ \t]*

  # Declare order of precedence
  INFIX_EXPRESSION(A, O) <-  A (O A)* {
    precedence
      L + -
      L * /
  }
)");

parser["INFIX_EXPRESSION"] = [](const SemanticValues& vs) -> long {
  auto result = any_cast<long>(vs[0]);
  if (vs.size() > 1) {
    auto ope = any_cast<char>(vs[1]);
    auto num = any_cast<long>(vs[2]);
    switch (ope) {
      case '+': result += num; break;
      case '-': result -= num; break;
      case '*': result *= num; break;
      case '/': result /= num; break;
    }
  }
  return result;
};
parser["OPERATOR"] = [](const SemanticValues& vs) { return *vs.sv(); };
parser["NUMBER"] = [](const SemanticValues& vs) { return vs.token_to_number<long>(); };

long val;
parser.parse(" -1 + (1 + 2) * 3 - -1", val);
assert(val == 9);
```

*precedence* instruction can be applied only to the following 'list' style rule.

```
Rule <- Atom (Operator Atom)* {
  precedence
    L - +
    L / *
    R ^
}
```

*precedence* instruction contains precedence info entries. Each entry starts with *associativity* which is 'L' (left) or 'R' (right), then operator *literal* tokens follow. The first entry has the highest order level.

AST generation
--------------

*cpp-peglib* is able to generate an AST (Abstract Syntax Tree) when parsing. `enable_ast` method on `peg::parser` class enables the feature.

NOTE: An AST node holds a corresponding token as `std::string_vew` for performance and less memory usage. It is users' responsibility to keep the original source text along with the generated AST tree.

```
peg::parser parser(R"(
  ...
  definition1 <- ... { no_ast_opt }
  definition2 <- ... { no_ast_opt }
  ...
)");

parser.enable_ast();

shared_ptr<peg::Ast> ast;
if (parser.parse("...", ast)) {
  cout << peg::ast_to_s(ast);

  ast = parser.optimize_ast(ast);
  cout << peg::ast_to_s(ast);
}
```

`optimize_ast` removes redundant nodes to make an AST simpler. If you want to disable this behavior from particular rules, `no_ast_opt` instruction can be used.

It internally calls `peg::AstOptimizer` to do the job. You can make your own AST optimizers to fit your needs.

See actual usages in the [AST calculator example](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc) and [PL/0 language example](https://github.com/yhirose/cpp-peglib/blob/master/pl0/pl0.cc).

Make a parser with parser combinators
-------------------------------------

Instead of making a parser by parsing PEG syntax text, we can also construct a parser by hand with *parser combinators*. Here is an example:

```cpp
using namespace peg;
using namespace std;

vector<string> tags;

Definition ROOT, TAG_NAME, _;
ROOT     <= seq(_, zom(seq(chr('['), TAG_NAME, chr(']'), _)));
TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const SemanticValues& vs) {
              tags.push_back(vs.token_to_string());
            };
_        <= zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

The following are available operators:

| Operator | Description                     | Operator | Description         |
|:---------|:--------------------------------|:---------|:--------------------|
| seq      | Sequence                        | cho      | Prioritized Choice  |
| zom      | Zero or More                    | oom      | One or More         |
| opt      | Optional                        | apd      | And predicate       |
| npd      | Not predicate                   | lit      | Literal string      |
| liti     | Case-insensitive Literal string | cls      | Character class     |
| ncls     | Negated Character class         | chr      | Character           |
| dot      | Any character                   | tok      | Token boundary      |
| ign      | Ignore semantic value           | csc      | Capture scope       |
| cap      | Capture                         | bkr      | Back reference      |
| dic      | Dictionary                      | pre      | Infix expression    |
| rec      | Infix expression                | usr      | User defined parser |
| rep      | Repetition                      |          |                     |

Adjust definitions
------------------

It's possible to add/override definitions.

```cpp
auto syntax = R"(
  ROOT <- _ 'Hello' _ NAME '!' _
)";

Rules additional_rules = {
  {
    "NAME", usr([](const char* s, size_t n, SemanticValues& vs, any& dt) -> size_t {
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

cpp-peglib accepts UTF8 text. `.` matches a Unicode codepoint. Also, it supports `\u????`.

Error report and recovery
-------------------------

cpp-peglib supports the furthest failure error position report as described in the Bryan Ford original document.

For better error report and recovery, cpp-peglib supports 'recovery' operator with label which can be associated with a recovery expression and a custom error message. This idea comes from the fantastic ["Syntax Error Recovery in Parsing Expression Grammars"](https://arxiv.org/pdf/1806.11150.pdf) paper by Sergio Medeiros and Fabio Mascarenhas.

The custom message supports `%t` which is a placeholder for the unexpected token, and `%c` for the unexpected Unicode char.

Here is an example of Java-like grammar:

```peg
# java.peg
Prog        ← 'public' 'class' NAME '{' 'public' 'static' 'void' 'main' '(' 'String' '[' ']' NAME ')' BlockStmt '}'
BlockStmt   ← '{' (!'}' Stmt^stmtb)* '}' # Annotated with `stmtb`
Stmt        ← IfStmt / WhileStmt / PrintStmt / DecStmt / AssignStmt / BlockStmt
IfStmt      ← 'if' '(' Exp ')' Stmt ('else' Stmt)?
WhileStmt   ← 'while' '(' Exp^condw ')' Stmt # Annotated with `condw`
DecStmt     ← 'int' NAME ('=' Exp)? ';'
AssignStmt  ← NAME '=' Exp ';'^semia # Annotated with `semi`
PrintStmt   ← 'System.out.println' '(' Exp ')' ';'
Exp         ← RelExp ('==' RelExp)*
RelExp      ← AddExp ('<' AddExp)*
AddExp      ← MulExp (('+' / '-') MulExp)*
MulExp      ← AtomExp (('*' / '/') AtomExp)*
AtomExp     ← '(' Exp ')' / NUMBER / NAME

NUMBER      ← < [0-9]+ >
NAME        ← < [a-zA-Z_][a-zA-Z_0-9]* >

%whitespace ← [ \t\n]*
%word       ← NAME

# Recovery operator labels
semia       ← '' { error_message "missing semicolon in assignment." }
stmtb       ← (!(Stmt / 'else' / '}') .)* { error_message "invalid statement" }
condw       ← &'==' ('==' RelExp)* / &'<' ('<' AddExp)* / (!')' .)*
```

For instance, `';'^semi` is a syntactic sugar for `(';' / %recovery(semi))`. `%recover` operator tries to recover the error at ';' by skipping input text with the recovery expression `semi`. Also `semi` is associated with a custom message "missing semicolon in assignment."

Here is the result:

```java
> cat sample.java
public class Example {
  public static void main(String[] args) {
    int n = 5;
    int f = 1;
    while( < n) {
      f = f * n;
      n = n - 1
    };
    System.out.println(f);
  }
}

> peglint java.peg sample.java
sample.java:5:12: syntax error, unexpected '<', expecting '(', <NUMBER>, <NAME>.
sample.java:8:5: missing semicolon in assignment.
sample.java:8:6: invalid statement
```

As you can see, it can now show more than one error, and provide more meaningful error messages than the default messages.

### Custom error message for definitions

We can associate custom error messages to definitions.

```peg
# custom_message.peg
START       <- CODE (',' CODE)*
CODE        <- < '0x' [a-fA-F0-9]+ > { error_message 'code format error...' }
%whitespace <- [ \t]*
```

```
> cat custom_message.txt
0x1234,0x@@@@,0xABCD

> peglint custom_message.peg custom_message.txt
custom_message.txt:1:8: code format error...
```

NOTE: If there is more than one element with an error message instruction in a prioritized choice, this feature may not work as you expect.

Errors During Actions
---------------------

It is possible to return a `peg::Error` during an action.

This can be used to check that a parsed number can be supported by the resulting integer type:
```cpp
parser parser(R"(
  NUMBER  <- [0-9]+
)");

parser.number = [](const peg::SemanticValues& vs) -> std::any {
  int value;
  auto [ptr, err] = std::from_chars(
    vs.token().data(),
    vs.token().data() + vs.token().size(),
    value
    );

  if (err != std::errc()) {
    return peg::Error("Number out of range.");
  }

  return value;
};
```

Change the Start Definition Rule
--------------------------------

We can change the start definition rule as below.

```cpp
auto grammar = R"(
  Start       <- A
  A           <- B (',' B)*
  B           <- '[one]' / '[two]'
  %whitespace <- [ \t\n]*
)";

peg::parser parser(grammar, "A"); // Start Rule is "A"

  or

peg::parser parser;
parser.load_grammar(grammar, "A"); // Start Rule is "A"

parser.parse(" [one] , [two] "); // OK
```

peglint - PEG syntax lint utility
---------------------------------

### Build peglint

```
> cd lint
> mkdir build
> cd build
> cmake ..
> make
> ./peglint
usage: grammar_file_path [source_file_path]

  options:
    --source: source text
    --packrat: enable packrat memoise
    --ast: show AST tree
    --opt, --opt-all: optimize all AST nodes except nodes selected with `no_ast_opt` instruction
    --opt-only: optimize only AST nodes selected with `no_ast_opt` instruction
    --trace: show concise trace messages
    --profile: show profile report
    --verbose: verbose output for trace and profile
```

### Grammar check

```
> cat a.peg
Additive    <- Multiplicative '+' Additive / Multiplicative
Multiplicative   <- Primary '*' Multiplicative / Primary
Primary     <- '(' Additive ')' / Number
%whitespace <- [ \t\r\n]*

> peglint a.peg
[commandline]:3:35: 'Number' is not defined.
```

### Source check

```
> cat a.peg
Additive    <- Multiplicative '+' Additive / Multiplicative
Multiplicative   <- Primary '*' Multiplicative / Primary
Primary     <- '(' Additive ')' / Number
Number      <- < [0-9]+ >
%whitespace <- [ \t\r\n]*

> peglint --source "1 + a * 3" a.peg
[commandline]:1:3: syntax error
```

### AST

```
> cat a.txt
1 + 2 * 3

> peglint --ast a.peg a.txt
+ Additive
  + Multiplicative
    + Primary
      - Number (1)
  + Additive
    + Multiplicative
      + Primary
        - Number (2)
      + Multiplicative
        + Primary
          - Number (3)
```

### AST optimization

```
> peglint --ast --opt --source "1 + 2 * 3" a.peg
+ Additive
  - Multiplicative[Number] (1)
  + Additive[Multiplicative]
    - Primary[Number] (2)
    - Multiplicative[Number] (3)
```

### Adjust AST optimization with `no_ast_opt` instruction

```
> cat a.peg
Additive    <- Multiplicative '+' Additive / Multiplicative
Multiplicative   <- Primary '*' Multiplicative / Primary
Primary     <- '(' Additive ')' / Number          { no_ast_opt }
Number      <- < [0-9]+ >
%whitespace <- [ \t\r\n]*

> peglint --ast --opt --source "1 + 2 * 3" a.peg
+ Additive/0
  + Multiplicative/1[Primary]
    - Number (1)
  + Additive/1[Multiplicative]
    + Primary/1
      - Number (2)
    + Multiplicative/1[Primary]
      - Number (3)

> peglint --ast --opt-only --source "1 + 2 * 3" a.peg
+ Additive/0
  + Multiplicative/1
    - Primary/1[Number] (1)
  + Additive/1
    + Multiplicative/0
      - Primary/1[Number] (2)
      + Multiplicative/1
        - Primary/1[Number] (3)
```

Sample codes
------------

* [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
* [Calculator (with parser operators)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
* [Calculator (AST version)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc3.cc)
* [Calculator (parsing expressions by precedence climbing)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc4.cc)
* [Calculator (AST version and parsing expressions by precedence climbing)](https://github.com/yhirose/cpp-peglib/blob/master/example/calc5.cc)
* [A tiny PL/0 JIT compiler in less than 900 LOC with LLVM and PEG parser](https://github.com/yhirose/pl0-jit-compiler)
* [A Programming Language just for writing Fizz Buzz program. :)](https://github.com/yhirose/fizzbuzzlang)

License
-------

MIT license (© 2022 Yuji Hirose)
