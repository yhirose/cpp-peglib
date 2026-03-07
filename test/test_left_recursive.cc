#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// ---------------------------------------------------------------------------
// Basic LR detection tests
// ---------------------------------------------------------------------------

TEST(LeftRecursionTest, Left_recursive_test) {
  parser parser(R"(
        A <- A 'a'
        B <- A 'a'
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionTest, Left_recursive_with_option_test) {
  parser parser(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionTest, Left_recursive_with_zom_test) {
  parser parser(R"(
        A <- 'a'* A*
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionTest, Left_recursive_with_a_ZOM_content_rule) {
  parser parser(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

  EXPECT_TRUE(parser);
}

TEST(LeftRecursionTest, Left_recursive_with_prefix_test) {
  parser parser(" A <- 'prefix' A / 'x'", "");

  EXPECT_TRUE(parser);
}

// ---------------------------------------------------------------------------
// Indirect / mutual / complex LR
// ---------------------------------------------------------------------------

TEST(LeftRecursionTest, Indirect_left_recursion_test) {
  parser parser(R"(
        A <- B 'a'
        B <- C 'b'
        C <- A 'c' / 'd'
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("dba"));
  EXPECT_TRUE(parser.parse("dbacba"));
}

TEST(LeftRecursionTest, Complex_indirect_left_recursion_test) {
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
  EXPECT_TRUE(parser.parse("hfecadbgfeca"));
  EXPECT_TRUE(parser.parse("hfecadbgfecadb"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("h"));
  EXPECT_FALSE(parser.parse("hf"));
  EXPECT_FALSE(parser.parse("abc"));
}

TEST(LeftRecursionTest, Mutual_left_recursion_test) {
  parser parser(R"(
        A <- B 'x' / 'a'
        B <- A 'y'
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));
  EXPECT_TRUE(parser.parse("ayx"));
  EXPECT_TRUE(parser.parse("ayxyx"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("b"));
  EXPECT_FALSE(parser.parse("ax"));
  EXPECT_FALSE(parser.parse("ay"));
}

// ---------------------------------------------------------------------------
// Epsilon-hidden left recursion
// ---------------------------------------------------------------------------

TEST(LeftRecursionTest, Hidden_left_recursion_with_epsilon_test) {
  parser parser(R"(
        A <- B A / 'a'
        B <- 'b'?
  )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));
  EXPECT_TRUE(parser.parse("ba"));
  EXPECT_TRUE(parser.parse("bba"));
  EXPECT_TRUE(parser.parse("bbba"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("b"));
  EXPECT_FALSE(parser.parse("ab"));
  EXPECT_FALSE(parser.parse("bb"));
}

TEST(LeftRecursionTest, Deep_indirect_epsilon_test) {
  // LR through a chain of rules with a nullable prefix
  parser parser(R"(
        A <- B
        B <- C
        C <- D? A / 'x'
        D <- 'x'
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("x"));
  EXPECT_TRUE(parser.parse("xx"));
}

TEST(LeftRecursionTest, Predicate_epsilon_test) {
  // &B is always nullable (predicates don't consume)
  parser parser(R"(
        A <- &B A / 'a'
        B <- 'b'?
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));
}

TEST(LeftRecursionTest, Complex_choice_epsilon_test) {
  // D <- '' makes D? A nullable prefix
  parser parser(R"(
        A <- B / C
        B <- D? A 'x'
        C <- 'y'
        D <- ''
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("y"));
  EXPECT_TRUE(parser.parse("yx"));
  EXPECT_TRUE(parser.parse("yxx"));
}

TEST(LeftRecursionTest, Complex_sequence_epsilon_test) {
  // Multiple nullable prefixes before the recursive reference
  parser parser(R"(
        A <- B C D A / 'a'
        B <- 'b'?
        C <- 'c'*
        D <- ''
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));
  EXPECT_TRUE(parser.parse("ba"));
  EXPECT_TRUE(parser.parse("bca"));
  EXPECT_TRUE(parser.parse("bcca"));
}

TEST(LeftRecursionTest, Cross_reference_epsilon_test) {
  // B is nullable through a chain of nullable rules (C->E->'e'?, D->F->'f'*)
  // PEG ordered choice: B tries C first, C=E='e'? always succeeds (empty),
  // so D is never reached. B effectively always matches empty.
  parser parser(R"(
        A <- B A / 'a'
        B <- C / D
        C <- E
        D <- F
        E <- 'e'?
        F <- 'f'*
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("a"));
  EXPECT_TRUE(parser.parse("ea"));
  // "fa" fails because B matches empty via C (ordered choice), not 'f' via D
  EXPECT_FALSE(parser.parse("fa"));
}

TEST(LeftRecursionTest, Pure_epsilon_left_recursion_test) {
  // A <- A / '' — most dangerous pattern, should not hang
  parser parser(R"(
        A <- A / ''
    )");

  EXPECT_TRUE(parser);

  // Grammar matches empty string
  EXPECT_TRUE(parser.parse(""));
  EXPECT_FALSE(parser.parse("a"));
}

TEST(LeftRecursionTest, Direct_left_recursion_no_alternatives_test) {
  // A <- A — no base case, can never match
  parser parser(R"(
        A <- A
    )");

  EXPECT_TRUE(parser);

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("a"));
}

TEST(LeftRecursionTest, Mutual_recursion_with_epsilon_test) {
  parser parser(R"(
        A <- B / ''
        B <- A / 'x'
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse(""));
  EXPECT_TRUE(parser.parse("x"));
}

TEST(LeftRecursionTest, Diamond_dependency_epsilon_test) {
  // Both B and C reference A through nullable D
  parser parser(R"(
        A <- B / C
        B <- D A 'b'
        C <- D A 'c'
        D <- 'x'?
    )");

  EXPECT_TRUE(parser);

  // No non-recursive base case, so nothing can parse
  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("a"));
}

TEST(LeftRecursionTest, Class_epsilon_test) {
  // [a-z]? is nullable, so A is left-recursive through it
  parser parser(R"(
        A <- [a-z]? A / 'x'
    )");

  EXPECT_TRUE(parser);

  EXPECT_TRUE(parser.parse("x"));
  EXPECT_TRUE(parser.parse("ax"));
  EXPECT_TRUE(parser.parse("abx"));
}

TEST(LeftRecursionTest, Epsilon_with_complex_predicate_test) {
  // &B succeeds (B='', always matches), !C succeeds when C fails
  parser parser(R"(
        A <- &B !C A / 'a'
        B <- ''
        C <- 'c'?
    )");

  EXPECT_TRUE(parser);

  // !C always fails because C <- 'c'? always succeeds (matches empty).
  // So &B !C A always fails, and A falls through to 'a'.
  EXPECT_TRUE(parser.parse("a"));
}

// ---------------------------------------------------------------------------
// Arithmetic expressions with semantic values
// ---------------------------------------------------------------------------

auto PEG_ArithmeticExpressions_Left_Recursion = R"(
  expr <- expr '+' term / expr '-' term / term
  term <- term '*' factor / term '/' factor / factor
  factor <- '(' expr ')' / number
  number <- [0-9]+
  %whitespace <- [ \t]*
)";

static void setup_arithmetic_actions(parser &p) {
  p["expr"] = [](const SemanticValues &vs) -> long {
    if (vs.choice() == 0) {
      return std::any_cast<long>(vs[0]) + std::any_cast<long>(vs[1]);
    } else if (vs.choice() == 1) {
      return std::any_cast<long>(vs[0]) - std::any_cast<long>(vs[1]);
    } else {
      return std::any_cast<long>(vs[0]);
    }
  };

  p["term"] = [](const SemanticValues &vs) -> long {
    if (vs.choice() == 0) {
      return std::any_cast<long>(vs[0]) * std::any_cast<long>(vs[1]);
    } else if (vs.choice() == 1) {
      return std::any_cast<long>(vs[0]) / std::any_cast<long>(vs[1]);
    } else {
      return std::any_cast<long>(vs[0]);
    }
  };

  p["factor"] = [](const SemanticValues &vs) -> long {
    return std::any_cast<long>(vs[0]);
  };

  p["number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };
}

TEST(LeftRecursionTest, ArithmeticExpressions_left_recursion_disabled) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_FALSE(p);
}

TEST(LeftRecursionTest, ArithmeticExpressions) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  setup_arithmetic_actions(p);

  long val;
  EXPECT_TRUE(p.parse("1+2*3-4*5+6/2", val));
  EXPECT_EQ(-10, val);
}

TEST(LeftRecursionTest, LeftAssociativity) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  setup_arithmetic_actions(p);

  long val;

  // 1-2-3 must be (1-2)-3 = -4 (left associative)
  // Right associative would give 1-(2-3) = 2
  EXPECT_TRUE(p.parse("1-2-3", val));
  EXPECT_EQ(-4, val);

  // 8/4/2 must be (8/4)/2 = 1 (left associative)
  // Right associative would give 8/(4/2) = 4
  EXPECT_TRUE(p.parse("8/4/2", val));
  EXPECT_EQ(1, val);
}

TEST(LeftRecursionTest, ArithmeticExpressions_with_AST) {
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

// ---------------------------------------------------------------------------
// Monkey language (cascading LR)
// ---------------------------------------------------------------------------

auto PEG_Monkey_Left_Recursion = R"(
# Monkey Language Grammar with Left Recursion
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

TEST(LeftRecursionTest, MonkeyLanguage) {
  parser p(PEG_Monkey);
  EXPECT_TRUE(p);

  EXPECT_TRUE(p.parse(Monkey_fibonacci));
  EXPECT_TRUE(p.parse(Monkey_closure));
  EXPECT_TRUE(p.parse(Monkey_map));
}

TEST(LeftRecursionTest, MonkeyLanguageLeftRecursion) {
  parser p(PEG_Monkey_Left_Recursion);
  EXPECT_TRUE(p);

  EXPECT_TRUE(p.parse(Monkey_fibonacci));
  EXPECT_TRUE(p.parse(Monkey_closure));
  EXPECT_TRUE(p.parse(Monkey_map));
}

// ---------------------------------------------------------------------------
// Non-LR regression (right-recursive expression, should not break with LR on)
// ---------------------------------------------------------------------------

TEST(LeftRecursionTest, RightRecursiveExpressionRegression) {
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

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("1 +"));
}

TEST(LeftRecursionTest, TypeScriptTypes_realistic) {
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
  EXPECT_TRUE(parser.parse("(string, number) => boolean"));
  EXPECT_TRUE(parser.parse("{name: string, age: number}"));

  EXPECT_FALSE(parser.parse(""));
  EXPECT_FALSE(parser.parse("|"));
}

// ---------------------------------------------------------------------------
// Packrat + LR integration
// ---------------------------------------------------------------------------

TEST(LeftRecursionTest, PackratWithLeftRecursion) {
  parser p(PEG_ArithmeticExpressions_Left_Recursion);
  EXPECT_TRUE(p);

  setup_arithmetic_actions(p);

  // Without packrat
  long val;
  EXPECT_TRUE(p.parse("1+2*3-4*5+6/2", val));
  EXPECT_EQ(-10, val);

  // With packrat
  p.enable_packrat_parsing();

  EXPECT_TRUE(p.parse("1+2*3-4*5+6/2", val));
  EXPECT_EQ(-10, val);

  // Multiple parses with packrat for consistency
  EXPECT_TRUE(p.parse("1+2+3+4", val));
  EXPECT_EQ(10, val);

  long val2;
  EXPECT_TRUE(p.parse("1+2+3+4", val2));
  EXPECT_EQ(val, val2);
}

// =============================================================================
// Left Recursion Detection Tests (LR disabled)
// =============================================================================

TEST(LeftRecursiveTest, Left_recursive_test) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(R"(
        A <- A 'a'
        B <- A 'a'
    )");

  EXPECT_FALSE(p);
}

TEST(LeftRecursiveTest, Left_recursive_with_option_test) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

  EXPECT_FALSE(p);
}

TEST(LeftRecursiveTest, Left_recursive_with_zom_test) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(R"(
        A <- 'a'* A*
    )");

  EXPECT_FALSE(p);
}

TEST(LeftRecursiveTest, Left_recursive_with_a_ZOM_content_rule) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

  EXPECT_FALSE(p);
}

TEST(LeftRecursiveTest, Left_recursive_with_empty_string_test) {
  parser p;
  p.enable_left_recursion(false);
  p.load_grammar(" A <- '' A");

  EXPECT_FALSE(p);
}
