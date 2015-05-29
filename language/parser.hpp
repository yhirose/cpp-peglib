#include <peglib.h>

enum AstType
{
   Statements, While, If, FunctionCall, Assignment, Condition, BinExpresion,
   Identifier, InterpolatedString,
   Number, Boolean, Function
};

peglib::peg& get_parser();

// vim: et ts=4 sw=4 cin cino={1s ff=unix
