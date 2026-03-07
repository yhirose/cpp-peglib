#include <chrono>
#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

TEST(LeftRecursionText, Left_recursive_test) {
  parser parser(R"(
        A <- A 'a'
        B <- A 'a'
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionText, Left_recursive_with_option_test) {
  parser parser(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionText, Left_recursive_with_zom_test) {
  parser parser(R"(
        A <- 'a'* A*
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionText, Left_recursive_with_a_ZOM_content_rule) {
  parser parser(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionText, Left_recursive_with_prefix_test) {
  parser parser(" A <- 'prefix' A / 'x'", "");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionText, Indirect_left_recursion_test) {
  parser parser(R"(
        A <- B 'a'
        B <- C 'b'
        C <- A 'c' / 'd'
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("dba"));

  // TODO: This should be valid, but currently fails due to left recursion
  // EXPECT_TRUE(parser.parse("dbacba"));
}

TEST(LeftRecursionText, Complex_indirect_left_recursion_test) {
  parser parser(R"(
        A <- B / C 'a'
        B <- D 'b'
        C <- E 'c'
        D <- A 'd'
        E <- F 'e'
        F <- G 'f'
        G <- A 'g' / 'h'
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("hfeca"));
  EXPECT_TRUE(parser.parse("hfecadb"));
  EXPECT_TRUE(parser.parse("hfecadbgfeca"));   // More complex left recursion
  EXPECT_TRUE(parser.parse("hfecadbgfecadb")); // Even more complex

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("h"));
  EXPECT_FALSE(parser.parse("hf"));
  EXPECT_FALSE(parser.parse("abc"));
}

TEST(LeftRecursionText, Mutual_left_recursion_test) {
  parser parser(R"(
        A <- B 'x' / 'a'
        B <- A 'y'
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));   // A -> 'a'
  EXPECT_TRUE(parser.parse("ayx")); // A -> 'a', B -> A'y' = ay, A -> B'x' = ayx
  EXPECT_TRUE(parser.parse("ayxyx")); // More complex mutual recursion

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("b"));
  EXPECT_FALSE(parser.parse("ax"));
  EXPECT_FALSE(parser.parse("ay"));
}

TEST(LeftRecursionText, Hidden_left_recursion_with_epsilon_test) {
  parser parser(R"(
        A <- B A / 'a'
        B <- 'b'?
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));  // A -> 'a'
  EXPECT_TRUE(parser.parse("ba")); // B -> 'b', A -> 'a', so B A = b a = ba
  EXPECT_TRUE(
      parser.parse("bba")); // B -> 'b', A -> B A (B->b, A->a) = b b a = bba
  EXPECT_TRUE(parser.parse("bbba")); // More recursive combinations

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("b"));
  EXPECT_FALSE(parser.parse("ab"));
  EXPECT_FALSE(parser.parse("bb"));
}

TEST(LeftRecursionText, DISABLED_Deep_indirect_epsilon_test) {
  parser parser(R"(
        A <- B
        B <- C
        C <- D? A
        D <- 'x'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Predicate_epsilon_test) {
  parser parser(R"(
        A <- &B A / 'a'
        B <- 'b'?
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Complex_choice_epsilon_test) {
  parser parser(R"(
        A <- B / C
        B <- D? A 'x'
        C <- 'y'
        D <- ''
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Multi_rule_epsilon_chain_test) {
  parser parser(R"(
        A <- B
        B <- C
        C <- D
        D <- E? A
        E <- 'e'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Not_predicate_epsilon_test) {
  parser parser(R"(
        A <- !B A / 'a'
        B <- 'b'?
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Token_boundary_epsilon_test) {
  parser parser(R"(
        A <- < B > A / 'a'
        B <- 'b'?
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Class_epsilon_test) {
  parser parser(R"(
        A <- [a-z]? A / 'x'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_All_epsilon_alternatives_test) {
  parser parser(R"(
        A <- (B / C / D) A / 'a'
        B <- 'b'?
        C <- 'c'*
        D <- ''
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Complex_sequence_epsilon_test) {
  parser parser(R"(
        A <- B C D A / 'a'
        B <- 'b'?
        C <- 'c'*
        D <- ''
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Cross_reference_epsilon_test) {
  parser parser(R"(
        A <- B A / 'a'
        B <- C / D
        C <- E
        D <- F
        E <- 'e'?
        F <- 'f'*
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Multiple_predicates_epsilon_test) {
  parser parser(R"(
        A <- &B &C A / 'a'
        B <- 'b'?
        C <- 'c'*
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Very_deep_chain_epsilon_test) {
  parser parser(R"(
        A <- B
        B <- C
        C <- D
        D <- E
        E <- F
        F <- G? A
        G <- 'g'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Diamond_dependency_epsilon_test) {
  parser parser(R"(
        A <- B / C
        B <- D A
        C <- D A
        D <- 'x'?
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Pure_epsilon_left_recursion_test) {
  // A <- A / '' is the most dangerous pattern
  parser parser(R"(
        A <- A / ''
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Direct_left_recursion_no_alternatives_test) {
  parser parser(R"(
        A <- A
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Mutual_recursion_with_epsilon_test) {
  parser parser(R"(
        A <- B / ''
        B <- A / 'x'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Complex_epsilon_chain_test) {
  parser parser(R"(
        A <- B
        B <- C
        C <- '' A
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Epsilon_with_complex_predicate_test) {
  parser parser(R"(
        A <- &B !C A / 'a'
        B <- ''
        C <- 'c'?
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Nested_epsilon_recursion_test) {
  parser parser(R"(
        A <- (B / '') A / 'x'
        B <- ''
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursionText, DISABLED_Triple_mutual_epsilon_test) {
  parser parser(R"(
        A <- B / ''
        B <- C / ''
        C <- A / 'z'
    )");

  EXPECT_FALSE(parser);
}

auto PEG_ArithmeticExpressions_Left_Recursion = R"(
  expr <- expr '+' term / expr '-' term / term
  term <- term '*' factor / term '/' factor / factor
  factor <- '(' expr ')' / number
  number <- [0-9]+
  %whitespace <- [ \t]*
)";

TEST(LeftRecursionText, ArithmeticExpressions_left_recursion_disabled) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion, "", false);
  EXPECT_FALSE(p);
}

TEST(LeftRecursionText, ArithmeticExpressions) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  p["expr"] = [](const SemanticValues &vs) -> long {
    if (vs.choice() == 0) { // expr '+' term
      return std::any_cast<long>(vs[0]) + std::any_cast<long>(vs[1]);
    } else if (vs.choice() == 1) { // expr '-' term
      return std::any_cast<long>(vs[0]) - std::any_cast<long>(vs[1]);
    } else { // term
      return std::any_cast<long>(vs[0]);
    }
  };

  p["term"] = [](const SemanticValues &vs) -> long {
    if (vs.choice() == 0) { // term '*' factor
      return std::any_cast<long>(vs[0]) * std::any_cast<long>(vs[1]);
    } else if (vs.choice() == 1) { // term '/' factor
      return std::any_cast<long>(vs[0]) / std::any_cast<long>(vs[1]);
    } else { // factor
      return std::any_cast<long>(vs[0]);
    }
  };

  p["factor"] = [](const SemanticValues &vs) -> long {
    if (vs.choice() == 0) { // '(' expr ')'
      return std::any_cast<long>(vs[0]);
    } else { // number
      return std::any_cast<long>(vs[0]);
    }
  };

  p["number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  long val;
  EXPECT_TRUE(p.parse("1+2*3-4*5+6/2", val));
  EXPECT_EQ(-10, val);
}

TEST(LeftRecursionText, ArithmeticExpressions_with_AST) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion);

  EXPECT_TRUE(p);

  p.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(p.parse("1+2*3-4*5+6/2", ast));

  ast = p.optimize_ast(ast);
  auto s = ast_to_s(ast);

  EXPECT_EQ(R"(+ expr/0
  + expr/1
    + expr/0
      - expr/2[number] (1)
      + term/0
        - term/2[number] (2)
        - factor/1[number] (3)
    + term/0
      - term/2[number] (4)
      - factor/1[number] (5)
  + term/1
    - term/2[number] (6)
    - factor/1[number] (2)
)",
            s);
}

auto PEG_Monkey_Left_Recursion = R"(
# Monkey Language Grammar with Left Recursion
# Based on the original Monkey language by Thorsten Ball
# Converted to use left recursion for better expression parsing

PROGRAM                <-  STATEMENTS

STATEMENTS             <-  (STATEMENT ';'?)*
STATEMENT              <-  ASSIGNMENT / RETURN / EXPRESSION_STATEMENT

ASSIGNMENT             <-  'let' IDENTIFIER '=' EXPRESSION
RETURN                 <-  'return' EXPRESSION
EXPRESSION_STATEMENT   <-  EXPRESSION

# Left-recursive expression grammar for proper operator precedence
EXPRESSION             <-  logical_expr
logical_expr           <-  logical_expr logical_op equality_expr / equality_expr
equality_expr          <-  equality_expr equality_op comparison_expr / comparison_expr
comparison_expr        <-  comparison_expr comparison_op additive_expr / additive_expr
additive_expr          <-  additive_expr additive_op multiplicative_expr / multiplicative_expr
multiplicative_expr    <-  multiplicative_expr multiplicative_op prefix_expr / prefix_expr
prefix_expr            <-  PREFIX_OPE* postfix_expr
postfix_expr           <-  postfix_expr (ARGUMENTS / INDEX) / primary_expr
primary_expr           <-  IF / FUNCTION / ARRAY / HASH / INTEGER / BOOLEAN / NULL / IDENTIFIER / STRING / '(' EXPRESSION ')'

# Operators with proper precedence levels
logical_op             <-  '&&' / '||'
equality_op            <-  '==' / '!='
comparison_op          <-  '<=' / '>=' / '<' / '>'
additive_op            <-  '+' / '-'
multiplicative_op      <-  '*' / '/'

IF                     <-  'if' '(' EXPRESSION ')' BLOCK ('else' BLOCK)?

FUNCTION               <-  'fn' '(' PARAMETERS ')' BLOCK
PARAMETERS             <-  LIST(IDENTIFIER, ',')

BLOCK                  <-  '{' STATEMENTS '}'

# Function calls and array indexing are handled in postfix_expr
ARGUMENTS              <-  '(' LIST(EXPRESSION, ',') ')'
INDEX                  <-  '[' EXPRESSION ']'

ARRAY                  <-  '[' LIST(EXPRESSION, ',') ']'

HASH                   <-  '{' LIST(HASH_PAIR, ',') '}'
HASH_PAIR              <-  EXPRESSION ':' EXPRESSION

IDENTIFIER             <-  !KEYWORD < [a-zA-Z_][a-zA-Z0-9_]* >
INTEGER                <-  < [0-9]+ >
STRING                 <-  < ["] (!["] .)* ["] >
BOOLEAN                <-  'true' / 'false'
NULL                   <-  'null'
PREFIX_OPE             <-  < [-!] >

KEYWORD                <-  'null' / 'true' / 'false' / 'let' / 'return' / 'if' / 'else' / 'fn'

LIST(ITEM, DELM)       <-  (ITEM (~DELM ITEM)*)?

LINE_COMMENT           <-  '//' (!LINE_END .)* &LINE_END
LINE_END               <-  '\r\n' / '\r' / '\n' / !.

%whitespace            <-  ([ \t\r\n]+ / LINE_COMMENT)*
%word                  <-  [a-zA-Z_][a-zA-Z0-9_]*
)";

auto PEG_Monkey = R"(
PROGRAM                <-  STATEMENTS

STATEMENTS             <-  (STATEMENT ';'?)*
STATEMENT              <-  ASSIGNMENT / RETURN / EXPRESSION_STATEMENT

ASSIGNMENT             <-  'let' IDENTIFIER '=' EXPRESSION
RETURN                 <-  'return' EXPRESSION
EXPRESSION_STATEMENT   <-  EXPRESSION

EXPRESSION             <-  INFIX_EXPR(PREFIX_EXPR, INFIX_OPE)
INFIX_EXPR(ATOM, OPE)  <-  ATOM (OPE ATOM)* {
                             precedence
                               L == !=
                               L < >
                               L + -
                               L * /
                           }

IF                     <-  'if' '(' EXPRESSION ')' BLOCK ('else' BLOCK)?

FUNCTION               <-  'fn' '(' PARAMETERS ')' BLOCK
PARAMETERS             <-  LIST(IDENTIFIER, ',')

BLOCK                  <-  '{' STATEMENTS '}'

CALL                   <-  PRIMARY (ARGUMENTS / INDEX)*
ARGUMENTS              <-  '(' LIST(EXPRESSION, ',') ')'
INDEX                  <-   '[' EXPRESSION ']'

PREFIX_EXPR            <-  PREFIX_OPE* CALL
PRIMARY                <-  IF / FUNCTION / ARRAY / HASH / INTEGER / BOOLEAN / NULL / IDENTIFIER / STRING / '(' EXPRESSION ')'

ARRAY                  <-  '[' LIST(EXPRESSION, ',') ']'

HASH                   <-  '{' LIST(HASH_PAIR, ',') '}'
HASH_PAIR              <-  EXPRESSION ':' EXPRESSION

IDENTIFIER             <-  < [a-zA-Z]+ >
INTEGER                <-  < [0-9]+ >
STRING                 <-  < ["] < (!["] .)* > ["] >
BOOLEAN                <-  'true' / 'false'
NULL                   <-  'null'
PREFIX_OPE             <-  < [-!] >
INFIX_OPE              <-  < [-+/*<>] / '==' / '!=' >

KEYWORD                <-  'null' | 'true' | 'false' | 'let' | 'return' | 'if' | 'else' | 'fn'

LIST(ITEM, DELM)       <-  (ITEM (~DELM ITEM)*)?

LINE_COMMENT           <-  '//' (!LINE_END .)* &LINE_END
LINE_END               <-  '\r\n' / '\r' / '\n' / !.

%whitespace            <-  ([ \t\r\n]+ / LINE_COMMENT)*
%word                  <-  [a-zA-Z]+
)";

auto Monkey_fibonacci = R"(
let fibonacci = fn(x) {
  if (x == 0) {
    0
  } else {
    if (x == 1) {
      return 1;
    } else {
      fibonacci(x - 1) + fibonacci(x - 2);
    }
  }
};

puts(fibonacci(35));
)";

auto Monkey_closure = R"(
// `newAdder` returns a closure that makes use of the free variables `a` and `b`:
let newAdder = fn(a, b) {
    fn(c) { a + b + c };
};

// This constructs a new `adder` function:
let adder = newAdder(1, 2);

puts(adder(8)); // => 11
)";

auto Monkey_map = R"(
let map = fn(arr, f) {
  let iter = fn(arr, accumulated) {
    if (len(arr) == 0) {
      accumulated
    } else {
      iter(rest(arr), push(accumulated, f(first(arr))));
    }
  };

  iter(arr, []);
};

let a = [1, 2, 3, 4];
let double = fn(x) { x * 2 };

puts(map(a, double));

let reduce = fn(arr, initial, f) {
  let iter = fn(arr, result) {
    if (len(arr) == 0) {
      result
    } else {
      iter(rest(arr), f(result, first(arr)));
    }
  };

  iter(arr, initial);
};

let sum = fn(arr) {
  reduce(arr, 0, fn(initial, el) { initial + el });
};

puts(sum([1, 2, 3, 4, 5]));
)";

TEST(LeftRecursionText, MonkeyLanguage) {
  parser p(PEG_Monkey);
  EXPECT_TRUE(p);

  EXPECT_TRUE(p.parse(Monkey_fibonacci));
  EXPECT_TRUE(p.parse(Monkey_closure));
  EXPECT_TRUE(p.parse(Monkey_map));
}

TEST(LeftRecursionText, MonkeyLanguageLeftRecursion) {
  parser p(PEG_Monkey_Left_Recursion);
  EXPECT_TRUE(p);

  EXPECT_TRUE(p.parse(Monkey_fibonacci));
  EXPECT_TRUE(p.parse(Monkey_closure));
  EXPECT_TRUE(p.parse(Monkey_map));
}

// Practical Programming Language Grammar Patterns
TEST(LeftRecursionText, ExpressionPrecedence_practical) {
  parser parser(R"(
        Expression  <- Term ('+' / '-') Expression / Term
        Term        <- Factor ('*' / '/') Term / Factor
        Factor      <- '(' Expression ')' / Number
        Number      <- < [0-9]+ >
        %whitespace <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("123"));
  EXPECT_TRUE(parser.parse("1 + 2"));
  EXPECT_TRUE(parser.parse("1 + 2 * 3"));
  EXPECT_TRUE(parser.parse("(1 + 2) * 3"));
  EXPECT_TRUE(parser.parse("1 + 2 * 3 - 4 / 5"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("+ 1"));
  EXPECT_FALSE(parser.parse("1 +"));
}

TEST(LeftRecursionText, TypeSystem_practical) {
  parser parser(R"(
        Type        <- FunctionType / ArrayType / PrimitiveType
        FunctionType <- '(' ParameterList ')' '=>' Type
        ArrayType   <- PrimitiveType ('[' ']')+
        ParameterList <- Type (',' Type)* / ''
        PrimitiveType <- 'int' / 'string' / 'bool'
        %whitespace <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("int"));
  EXPECT_TRUE(parser.parse("int[]"));
  EXPECT_TRUE(parser.parse("int[][]"));
  EXPECT_TRUE(parser.parse("(int, string) => bool"));
  EXPECT_TRUE(parser.parse("(int[], string) => bool[]"));
  EXPECT_TRUE(parser.parse("((int) => string, bool) => int[]"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("[]"));
  EXPECT_FALSE(parser.parse("() =>"));
}

TEST(LeftRecursionText, DeclarationSyntax_practical) {
  parser parser(R"(
        Declaration <- DeclarationSpecifier Declarator
        DeclarationSpecifier <- 'int' / 'char' / 'void'
        Declarator  <- '*'* DirectDeclarator
        DirectDeclarator <- Identifier ('[' ']')* / '(' Declarator ')' ('[' ']')*
        Identifier  <- < [a-zA-Z][a-zA-Z0-9]* >
        %whitespace <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("int x"));
  EXPECT_TRUE(parser.parse("int *x"));
  EXPECT_TRUE(parser.parse("int x[]"));
  EXPECT_TRUE(parser.parse("int *x[]"));
  EXPECT_TRUE(parser.parse("int (*x)[]"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("*"));
  EXPECT_FALSE(parser.parse("int"));
}

TEST(LeftRecursionText, JSONStructure_practical) {
  parser parser(R"(
        Value   <- Object / Array / String / Number / 'true' / 'false' / 'null'
        Object  <- '{' Members '}'
        Members <- Member (',' Member)* / ''
        Member  <- String ':' Value
        Array   <- '[' Elements ']'
        Elements <- Value (',' Value)* / ''
        String  <- '"' < [^"]* > '"'
        Number  <- < [0-9]+ >
        %whitespace <- [ \t\n]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("123"));
  EXPECT_TRUE(parser.parse("true"));
  EXPECT_TRUE(parser.parse("\"hello\""));
  EXPECT_TRUE(parser.parse("[]"));
  EXPECT_TRUE(parser.parse("[1, 2, 3]"));
  EXPECT_TRUE(parser.parse("{}"));
  EXPECT_TRUE(parser.parse("{\"key\": \"value\"}"));
  EXPECT_TRUE(parser.parse("{\"arr\": [1, 2], \"obj\": {\"nested\": true}}"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("{"));
  EXPECT_FALSE(parser.parse("[1,]"));
}

// Real Language Examples
TEST(LeftRecursionText, PythonExpressions_realistic) {
  parser parser(R"(
        expr       <- xor_expr ('|' xor_expr)*
        xor_expr   <- and_expr ('^' and_expr)*
        and_expr   <- shift_expr ('&' shift_expr)*
        shift_expr <- arith_expr (('<<' / '>>') arith_expr)*
        arith_expr <- term (('+' / '-') term)*
        term       <- factor (('*' / '/' / '%') factor)*
        factor     <- ('+' / '-')? power
        power      <- atom ('**' factor)?
        atom       <- Number / '(' expr ')'
        Number     <- < [0-9]+ >
        %whitespace <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("1"));
  EXPECT_TRUE(parser.parse("1 + 2"));
  EXPECT_TRUE(parser.parse("1 + 2 * 3"));
  EXPECT_TRUE(parser.parse("2 ** 3 ** 4")); // Right associative
  EXPECT_TRUE(parser.parse("1 << 2 + 3"));
  EXPECT_TRUE(parser.parse("1 | 2 & 3 ^ 4"));
  EXPECT_TRUE(parser.parse("(1 + 2) * (3 - 4)"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("1 +"));
  EXPECT_FALSE(parser.parse("**2"));
}

TEST(LeftRecursionText, TypeScriptTypes_realistic) {
  parser parser(R"(
        Type           <- UnionType / IntersectionType / PrimaryType
        UnionType      <- UnionType '|' IntersectionType / IntersectionType
        IntersectionType <- IntersectionType '&' PrimaryType / PrimaryType
        PrimaryType    <- BasicType / ObjectType / FunctionType / '(' Type ')'
        ObjectType     <- '{' PropertyList '}'
        PropertyList   <- Property (',' Property)* / ''
        Property       <- Identifier ':' Type
        FunctionType   <- '(' ParameterList ')' '=>' Type
        ParameterList  <- Type (',' Type)* / ''
        BasicType      <- 'string' / 'number' / 'boolean'
        Identifier     <- < [a-zA-Z][a-zA-Z0-9]* >
        %whitespace    <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("string"));
  EXPECT_TRUE(parser.parse("string | number"));
  EXPECT_TRUE(parser.parse("string & number"));
  EXPECT_FALSE(parser.parse("string[]"));
  EXPECT_TRUE(parser.parse("(string, number) => boolean"));
  EXPECT_TRUE(parser.parse("{name: string, age: number}"));
  EXPECT_FALSE(parser.parse("string | number[] | {id: string}"));
  EXPECT_FALSE(parser.parse("((string) => number) | boolean[]"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("|"));
  EXPECT_FALSE(parser.parse("string |"));
}

TEST(LeftRecursionText, CDeclarations_realistic) {
  parser parser(R"(
        Declaration    <- TypeSpecifier Declarator
        TypeSpecifier  <- 'int' / 'char' / 'void' / 'float' / 'double'
        Declarator     <- PointerDeclarator / DirectDeclarator
        PointerDeclarator <- '*' Declarator
        DirectDeclarator <- Identifier / '(' Declarator ')'
        Identifier     <- < [a-zA-Z_][a-zA-Z0-9_]* >
        %whitespace    <- [ \t]*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("int x"));
  EXPECT_TRUE(parser.parse("int *x"));
  EXPECT_TRUE(parser.parse("int **x"));
  EXPECT_FALSE(parser.parse("int x[]"));
  EXPECT_FALSE(parser.parse("int x[10]"));
  EXPECT_FALSE(parser.parse("int *x[]"));
  EXPECT_TRUE(parser.parse("int (*x)"));
  EXPECT_FALSE(parser.parse("int x()"));
  EXPECT_FALSE(parser.parse("int x(int y, char *z)"));
  EXPECT_FALSE(parser.parse("int (*func_ptr)(int, char*)"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("*x"));
  EXPECT_FALSE(parser.parse("int"));
}

TEST(LeftRecursionText, PackratParsingWithLeftRecursion) {
  // NOTE: This test exposes a limitation in our current implementation
  // where explicit packrat enabling affects left recursion parsing
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  // Test without packrat first
  EXPECT_TRUE(p.parse("1"));
  EXPECT_TRUE(p.parse("1+2"));
  EXPECT_TRUE(p.parse("1*2"));

  // Simple expressions work, now test with packrat
  p.enable_packrat_parsing();

  // Test same simple expressions with packrat enabled
  EXPECT_TRUE(p.parse("1"));
  EXPECT_TRUE(p.parse("1+2"));
  EXPECT_TRUE(p.parse("1*2"));
}

TEST(LeftRecursionText, PackratCacheBugFix) {
  // NOTE: This test exposes a limitation in our current implementation
  // where explicit packrat enabling affects left recursion parsing
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  // Test basic functionality first
  EXPECT_TRUE(p.parse("1"));
  EXPECT_TRUE(p.parse("1+2"));

  // Enable packrat parsing to test the cache bug fix
  p.enable_packrat_parsing();

  // Test expression that would trigger the cache bug
  std::string input = "1+2+3";

  // Parse multiple times to check for consistency
  bool result1 = p.parse(input);
  bool result2 = p.parse(input);
  bool result3 = p.parse(input);

  EXPECT_TRUE(result1);
  EXPECT_TRUE(result2);
  EXPECT_TRUE(result3);

  // Check consistency
  bool consistent = (result1 == result2) && (result2 == result3);
  EXPECT_TRUE(consistent);

  // All parses should succeed and be consistent
  EXPECT_TRUE(result1 && result2 && result3 && consistent);
}

TEST(LeftRecursionText, LeftRecursionArithmeticExpressionBasic) {
  // NOTE: This test exposes a limitation in our current implementation
  // where explicit packrat enabling affects left recursion parsing
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  // Test various expressions without packrat first
  EXPECT_TRUE(p.parse("1"));
  EXPECT_TRUE(p.parse("1+2"));
  EXPECT_TRUE(p.parse("1*2"));
  EXPECT_TRUE(p.parse("1+2*3"));
  EXPECT_TRUE(p.parse("(1+2)*3"));
  EXPECT_TRUE(p.parse("1+2*3+4"));
  EXPECT_TRUE(p.parse("1+2+3+4"));
  EXPECT_TRUE(p.parse("1*2*3*4"));
  EXPECT_TRUE(p.parse("((1+2)*3)"));

  // Test with packrat enabled
  p.enable_packrat_parsing();
  EXPECT_TRUE(p.parse("1+2*3+4"));
  EXPECT_TRUE(p.parse("1+2+3+4"));
  EXPECT_TRUE(p.parse("1*2*3*4"));
}

// Phase 1 Tests: Basic functionality for def_id mapping and cache management
TEST(LeftRecursionText, Phase1_DefIdMapping) {
  parser p(R"(
        START <- A / B / C
        A <- 'a'
        B <- 'b'
        C <- 'c'
        %whitespace <- [ ]*
    )");

  EXPECT_TRUE(p);

  // Test basic parsing works
  EXPECT_TRUE(p.parse("a"));
  EXPECT_TRUE(p.parse("b"));
  EXPECT_TRUE(p.parse("c"));
  EXPECT_FALSE(p.parse("d"));
}
