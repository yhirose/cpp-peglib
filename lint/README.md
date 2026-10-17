peglint
-------

The lint utility for PEG.

```
> peglint --help
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
    --hints: show hints to make parsing faster
    --blob: write a serialized grammar blob to stdout (load it later with
            peg::parser::load_blob to skip the meta-parse on startup)
    --version: show version information
```

### Build peglint

```
> cd lint
> mkdir build
> cd build
> cmake ..
> make
```

### Lint grammar

```
> cat a.peg
A <- 'hello' ^ 'world'

> peglint a.peg
a.peg:1:16: syntax error
```

```
> cat a.peg
A <- B

> peglint a.peg
a.peg:1:6: 'B' is not defined.
```

```
> cat a.peg
A <- ('a'?)*

> peglint a.peg
a.peg:1:1: infinite loop is detected in 'A'.
```

### Lint source text

```
> cat a.peg
Additive    <- Multiplicative '+' Additive / Multiplicative
Multiplicative   <- Primary '*' Multiplicative / Primary
Primary     <- '(' Additive ')' / Number
Number      <- < [0-9]+ >
%whitespace <- [ \t\r\n]*

> peglint --source "1 + a * 3" a.peg
[commandline]:1:5: syntax error, unexpected 'a', expecting '(', <Number>.
```

### AST

```
> cat a.txt
1 + 2 * 3

> peglint --ast a.peg a.txt
+ Additive/0
  + Multiplicative/1
    + Primary/1
      - Number (1)
  + Additive/1
    + Multiplicative/0
      + Primary/1
        - Number (2)
      + Multiplicative/1
        + Primary/1
          - Number (3)
```

### AST optimization

```
> peglint --ast --opt --source "1 + 2 * 3" a.peg
+ Additive/0
  - Multiplicative/1[Number] (1)
  + Additive/1[Multiplicative]
    - Primary/1[Number] (2)
    - Multiplicative/1[Number] (3)
```

`Rule/1` tells which alternative of the rule's choice matched, counting from 0, and `Rule[Other]` that the optimization put a node of `Other` in place of the node of `Rule`.

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
