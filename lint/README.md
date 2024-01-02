peglint
-------

The lint utility for PEG.

```
usage: grammar_file_path [source_file_path]

  options:
    --ast: show AST tree
    --packrat: enable packrat memoise
    --opt, --opt-all: optimaze all AST nodes except nodes selected with `no_ast_opt` instruction
    --opt-only: optimaze only AST nodes selected with `no_ast_opt` instruction
    --source: source text
    --trace: show concise trace messages
    --profile: show profile report
    --verbose: verbose output for trace and profile
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
A <- B / C
B <- 'b'
C <- A

> peglint a.peg
a.peg:1:10: 'C' is left recursive.
a.peg:3:6: 'A' is left recursive.
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
