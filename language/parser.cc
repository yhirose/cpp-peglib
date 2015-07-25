#include "parser.hpp"

using namespace peglib;
using namespace std;

const auto g_grammar = R"(

    PROGRAM                  <-  _ STATEMENTS

    STATEMENTS               <-  (EXPRESSION (';' _)?)*

    EXPRESSION               <-  ASSIGNMENT / LOGICAL_OR
    ASSIGNMENT               <-  MUTABLE IDENTIFIER '=' _ EXPRESSION
    WHILE                    <-  'while' _ EXPRESSION BLOCK
    IF                       <-  'if' _ EXPRESSION BLOCK ('else' _ 'if' _ EXPRESSION BLOCK)* ('else' _ BLOCK)?

    LOGICAL_OR               <-  LOGICAL_AND ('||' _ LOGICAL_AND)*
    LOGICAL_AND              <-  CONDITION ('&&' _  CONDITION)*
    CONDITION                <-  ADDITIVE (CONDITION_OPERATOR ADDITIVE)*
    ADDITIVE                 <-  UNARY_PLUS (ADDITIVE_OPERATOR UNARY_PLUS)*
    UNARY_PLUS               <-  UNARY_PLUS_OPERATOR? UNARY_MINUS
    UNARY_MINUS              <-  UNARY_MINUS_OPERATOR? UNARY_NOT
    UNARY_NOT                <-  UNARY_NOT_OPERATOR? MULTIPLICATIVE
    MULTIPLICATIVE           <-  CALL (MULTIPLICATIVE_OPERATOR CALL)*

    CALL                     <-  PRIMARY (ARGUMENTS / INDEX / DOT)*
    ARGUMENTS                <-  '(' _ (EXPRESSION (',' _ EXPRESSION)*)? ')' _
    INDEX                    <-  '[' _ EXPRESSION ']' _
    DOT                      <-  '.' _ IDENTIFIER

    PRIMARY                  <-  WHILE / IF / FUNCTION / IDENTIFIER / OBJECT / ARRAY / NUMBER / BOOLEAN / STRING / INTERPOLATED_STRING / '(' _ EXPRESSION ')' _

    FUNCTION                 <-  'fn' _ PARAMETERS BLOCK
    PARAMETERS               <-  '(' _ (PARAMETER (',' _ PARAMETER)*)? ')' _
    PARAMETER                <-  MUTABLE IDENTIFIER

    BLOCK                    <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR       <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    ADDITIVE_OPERATOR        <-  < [-+] > _
    UNARY_PLUS_OPERATOR      <-  < '+' > _
    UNARY_MINUS_OPERATOR     <-  < '-' > _
    UNARY_NOT_OPERATOR       <-  < '!' > _
    MULTIPLICATIVE_OPERATOR  <-  < [*/%] > _

    IDENTIFIER               <-  < [a-zA-Z_][a-zA-Z0-9_]* > _

    OBJECT                   <-  '{' _ (OBJECT_PROPERTY (',' _ OBJECT_PROPERTY)*)? '}' _
    OBJECT_PROPERTY          <-  IDENTIFIER ':' _ EXPRESSION

    ARRAY                    <-  '[' _ (EXPRESSION (',' _ EXPRESSION)*)? ']' _

    NUMBER                   <-  < [0-9]+ > _
    BOOLEAN                  <-  < ('true' / 'false') > _
    STRING                   <-  ['] < (!['] .)* > ['] _

    INTERPOLATED_STRING      <-  '"' ('{' _ EXPRESSION '}' / INTERPOLATED_CONTENT)* '"' _
    INTERPOLATED_CONTENT     <-  (!["{] .) (!["{] .)*

    MUTABLE                  <-  < 'mut'? > _

    ~_                       <-  (Space / EndOfLine / Comment)*
    Space                    <-  ' ' / '\t'
    EndOfLine                <-  '\r\n' / '\n' / '\r'
    EndOfFile                <-  !.
    Comment                  <-  '/*' (!'*/' .)* '*/' /  ('#' / '//') (!(EndOfLine / EndOfFile) .)* (EndOfLine / EndOfFile)

)";

peg& get_parser()
{
    static peg  parser;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        parser.log = [&](size_t ln, size_t col, const string& msg) {
            cerr << ln << ":" << col << ": " << msg << endl;
        };

        if (!parser.load_grammar(g_grammar)) {
            throw logic_error("invalid peg grammar");
        }

        parser.enable_ast(true, { "PARAMETERS", "ARGUMENTS" });
    }

    return parser;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
