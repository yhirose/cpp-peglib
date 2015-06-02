#include "parser.hpp"

using namespace peglib;
using namespace std;

static auto g_grammar = R"(
    PROGRAM               <-  _ STATEMENTS

    STATEMENTS            <-  EXPRESSION*

    EXPRESSION            <-  ASSIGNMENT / PRIMARY
    ASSIGNMENT            <-  IDENTIFIER '=' _ EXPRESSION
    WHILE                 <-  'while' _ EXPRESSION BLOCK
    IF                    <-  'if' _ EXPRESSION BLOCK ('else' _ 'if' _ EXPRESSION BLOCK)* ('else' _ BLOCK)?
    FUNCTION              <-  'fn' _ PARAMETERS BLOCK
    PARAMETERS            <-  '(' _ IDENTIFIER* ')' _ 
    FUNCTION_CALL         <-  IDENTIFIER ARGUMENTS
    ARGUMENTS             <-  '(' _ EXPRESSION* ')' _ 

    PRIMARY               <-  CONDITION (CONDITION_OPERATOR CONDITION)?
    CONDITION             <-  TERM (TERM_OPERATOR TERM)*
    TERM                  <-  FACTOR (FACTOR_OPERATOR FACTOR)*
    FACTOR                <-  WHILE / IF / FUNCTION / FUNCTION_CALL / NUMBER / BOOLEAN / STRING / INTERPOLATED_STRING / IDENTIFIER / '(' _ EXPRESSION ')' _

    BLOCK                 <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR    <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    TERM_OPERATOR         <-  < [-+] > _
    FACTOR_OPERATOR       <-  < [*/%] > _
    IDENTIFIER            <-  < [a-zA-Z_][a-zA-Z0-9_]* > _

    NUMBER                <-  < [0-9]+ > _
    BOOLEAN               <-  < ('true' / 'false') > _
    STRING                <-  ['] < (!['] .)* > ['] _

    INTERPOLATED_STRING   <-  '"' ('{' _ EXPRESSION '}' / INTERPOLATED_CONTENT)* '"' _
    INTERPOLATED_CONTENT  <-  (!["{] .) (!["{] .)*

    ~_                    <-  (Space / EndOfLine / Comment)*
    Space                 <-  ' ' / '\t'
    EndOfLine             <-  '\r\n' / '\n' / '\r'
    EndOfFile             <-  !.
    Comment               <-  '/*' (!'*/' .)* '*/' /  ('#' / '//') (!(EndOfLine / EndOfFile) .)* (EndOfLine / EndOfFile)
)";

peg& get_parser()
{
    static peg parser;

    static bool initialized = false;

    if (!initialized) {
        initialized = true;

        parser.log = [&](size_t ln, size_t col, const string& msg) {
            cerr << ln << ":" << col << ": " << msg << endl;
        };

        if (!parser.load_grammar(g_grammar)) {
            throw logic_error("invalid peg grammar");
        }

        parser.ast({
            { peg::AstNodeType::Regular,     "STATEMENTS",           Statements         },
            { peg::AstNodeType::Regular,     "WHILE",                While              },
            { peg::AstNodeType::Regular,     "ASSIGNMENT",           Assignment         },
            { peg::AstNodeType::Regular,     "IF",                   If                 },
            { peg::AstNodeType::Regular,     "FUNCTION",             Function           },
            { peg::AstNodeType::Regular,     "PARAMETERS",           Undefined          },
            { peg::AstNodeType::Regular,     "FUNCTION_CALL",        FunctionCall       },
            { peg::AstNodeType::Regular,     "ARGUMENTS",            Undefined          },
            { peg::AstNodeType::Optimizable, "PRIMARY",              Condition          },
            { peg::AstNodeType::Optimizable, "CONDITION",            BinExpresion       },
            { peg::AstNodeType::Optimizable, "TERM",                 BinExpresion       },
            { peg::AstNodeType::Token,       "CONDITION_OPERATOR",   Undefined          },
            { peg::AstNodeType::Token,       "TERM_OPERATOR",        Undefined          },
            { peg::AstNodeType::Token,       "FACTOR_OPERATOR",      Undefined          },
            { peg::AstNodeType::Token,       "NUMBER",               Number             },
            { peg::AstNodeType::Token,       "BOOLEAN",              Boolean            },
            { peg::AstNodeType::Token,       "STRING",               Undefined          },
            { peg::AstNodeType::Token,       "IDENTIFIER",           Identifier         },
            { peg::AstNodeType::Regular,     "INTERPOLATED_STRING",  InterpolatedString },
            { peg::AstNodeType::Token,       "INTERPOLATED_CONTENT", Undefined          },
        },
        Undefined);
    }

    return parser;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
