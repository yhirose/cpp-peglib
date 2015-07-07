#include "parser.hpp"

using namespace peglib;
using namespace std;

static auto g_grammar = R"(

    PROGRAM               <-  _ STATEMENTS

    STATEMENTS            <-  (EXPRESSION (';' _)?)*

    EXPRESSION            <-  ASSIGNMENT / LOGICAL_OR
    ASSIGNMENT            <-  MUTABLE IDENTIFIER '=' _ EXPRESSION
    WHILE                 <-  'while' _ EXPRESSION BLOCK
    IF                    <-  'if' _ EXPRESSION BLOCK ('else' _ 'if' _ EXPRESSION BLOCK)* ('else' _ BLOCK)?
    FUNCTION_CALL         <-  IDENTIFIER ARGUMENTS
    ARGUMENTS             <-  '(' _ (EXPRESSION (', ' _ EXPRESSION)*)? ')' _

    LOGICAL_OR            <-  LOGICAL_AND ('||' _ LOGICAL_AND)*
    LOGICAL_AND           <-  CONDITION ('&&' _  CONDITION)*
    CONDITION             <-  ADDITIVE (CONDITION_OPERATOR ADDITIVE)*
    ADDITIVE              <-  UNARY_PLUS (ADDITIVE_OPERATOR UNARY_PLUS)*
    UNARY_PLUS            <-  UNARY_PLUS_OPERATOR? UNARY_MINUS
    UNARY_MINUS           <-  UNARY_MINUS_OPERATOR? UNARY_NOT
    UNARY_NOT             <-  UNARY_NOT_OPERATOR? MULTIPLICATIVE
    MULTIPLICATIVE        <-  PRIMARY (MULTIPLICATIVE_OPERATOR PRIMARY)*
    PRIMARY               <-  WHILE / IF / FUNCTION / FUNCTION_CALL / ARRAY / ARRAY_REFERENCE / NUMBER / BOOLEAN / STRING / INTERPOLATED_STRING / IDENTIFIER / '(' _ EXPRESSION ')' _

    FUNCTION              <-  'fn' _ PARAMETERS BLOCK
    PARAMETERS            <-  '(' _ (PARAMETER (',' _ PARAMETER)*)? ')' _
    PARAMETER             <-  MUTABLE IDENTIFIER

    ARRAY                 <-  '[' _ (EXPRESSION (',' _ EXPRESSION)*) ']' _
    ARRAY_REFERENCE       <-  IDENTIFIER '[' _ EXPRESSION ']' _

    BLOCK                 <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR    <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    ADDITIVE_OPERATOR     <-  < [-+] > _
    UNARY_PLUS_OPERATOR   <-  < '+' > _
    UNARY_MINUS_OPERATOR  <-  < '-' > _
    UNARY_NOT_OPERATOR    <-  < '!' > _
    MULTIPLICATIVE_OPERATOR  <-  < [*/%] > _

    IDENTIFIER            <-  < [a-zA-Z_][a-zA-Z0-9_]* > _

    NUMBER                <-  < [0-9]+ > _
    BOOLEAN               <-  < ('true' / 'false') > _
    STRING                <-  ['] < (!['] .)* > ['] _

    INTERPOLATED_STRING   <-  '"' ('{' _ EXPRESSION '}' / INTERPOLATED_CONTENT)* '"' _
    INTERPOLATED_CONTENT  <-  (!["{] .) (!["{] .)*

    MUTABLE               <-  < 'mut'? > _

    ~_                    <-  (Space / EndOfLine / Comment)*
    Space                 <-  ' ' / '\t'
    EndOfLine             <-  '\r\n' / '\n' / '\r'
    EndOfFile             <-  !.
    Comment               <-  '/*' (!'*/' .)* '*/' /  ('#' / '//') (!(EndOfLine / EndOfFile) .)* (EndOfLine / EndOfFile)

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

        parser.enable_ast(true, {
            /*
               Definition,            Tag               Optimize
             ---------------------- ------------------ ---------- */
            { "STATEMENTS",          Statements,         false    },
            { "WHILE",               While,              false    },
            { "ASSIGNMENT",          Assignment,         false    },
            { "IF",                  If,                 false    },
            { "FUNCTION",            Function,           false    },
            { "PARAMETERS",          Default,            false    },
            { "FUNCTION_CALL",       FunctionCall,       false    },
            { "ARRAY",               Array,              false    },
            { "ARRAY_REFERENCE",     ArrayReference,     false    },
            { "ARGUMENTS",           Default,            false    },
            { "LOGICAL_OR",          LogicalOr,          true     },
            { "LOGICAL_AND",         LogicalAnd,         true     },
            { "CONDITION",           Condition,          true     },
            { "ADDITIVE",            BinExpresion,       true     },
            { "UNARY_PLUS",          UnaryPlus,          true     },
            { "UNARY_MINUS",         UnaryMinus,         true     },
            { "UNARY_NOT",           UnaryNot,           true     },
            { "MULTIPLICATIVE",      BinExpresion,       true     },
            { "NUMBER",              Number,             false    },
            { "BOOLEAN",             Boolean,            false    },
            { "IDENTIFIER",          Identifier,         false    },
            { "INTERPOLATED_STRING", InterpolatedString, false    },
        });
    }

    return parser;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
