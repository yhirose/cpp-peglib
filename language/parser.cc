#include "parser.hpp"

using namespace peglib;
using namespace std;

static auto g_grammar = R"(

    PROGRAM               <-  _ STATEMENTS

    STATEMENTS            <-  (EXPRESSION (';' _)?)*

    EXPRESSION            <-  ASSIGNMENT / PRIMARY
    ASSIGNMENT            <-  MUTABLE IDENTIFIER '=' _ EXPRESSION
    WHILE                 <-  'while' _ EXPRESSION BLOCK
    IF                    <-  'if' _ EXPRESSION BLOCK ('else' _ 'if' _ EXPRESSION BLOCK)* ('else' _ BLOCK)?
    FUNCTION              <-  'fn' _ PARAMETERS BLOCK
    PARAMETERS            <-  '(' _ (PARAMETER (',' _ PARAMETER)*)? ')' _
    PARAMETER             <-  MUTABLE IDENTIFIER
    FUNCTION_CALL         <-  IDENTIFIER ARGUMENTS
    ARGUMENTS             <-  '(' _ (EXPRESSION (', ' _ EXPRESSION)*)? ')' _

    PRIMARY               <-  LOGICAL_OR ('||' _ LOGICAL_OR)*
    LOGICAL_OR            <-  LOGICAL_AND ('&&' _  LOGICAL_AND)*
    LOGICAL_AND           <-  CONDITION (CONDITION_OPERATOR CONDITION)*
    CONDITION             <-  TERM (TERM_OPERATOR TERM)*
    TERM                  <-  UNARY_PLUS_OPERATOR? UNARY_PLUS
    UNARY_PLUS            <-  UNARY_MINUS_OPERATOR? UNARY_MINUS
    UNARY_MINUS           <-  UNARY_NOT_OPERATOR? UNARY_NOT
    UNARY_NOT             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
    FACTOR                <-  WHILE / IF / FUNCTION / FUNCTION_CALL / NUMBER / BOOLEAN / STRING / INTERPOLATED_STRING / IDENTIFIER / '(' _ EXPRESSION ')' _

    BLOCK                 <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR    <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    TERM_OPERATOR         <-  < [-+] > _
    UNARY_PLUS_OPERATOR   <-  < '+' > _
    UNARY_MINUS_OPERATOR  <-  < '-' > _
    UNARY_NOT_OPERATOR    <-  < '!' > _
    FACTOR_OPERATOR       <-  < [*/%] > _

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

        parser.enable_ast({
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
            { "ARGUMENTS",           Default,            false    },
            { "PRIMARY",             LogicalOr,          true     },
            { "LOGICAL_OR",          LogicalAnd,         true     },
            { "LOGICAL_AND",         Condition,          true     },
            { "CONDITION",           BinExpresion,       true     },
            { "TERM",                UnaryPlus,          true     },
            { "UNARY_PLUS",          UnaryMinus,         true     },
            { "UNARY_MINUS",         UnaryNot,           true     },
            { "UNARY_NOT",           BinExpresion,       true     },
            { "NUMBER",              Number,             false    },
            { "BOOLEAN",             Boolean,            false    },
            { "IDENTIFIER",          Identifier,         false    },
            { "INTERPOLATED_STRING", InterpolatedString, false    },
        });
    }

    return parser;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
