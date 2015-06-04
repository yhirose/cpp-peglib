#include <peglib.h>

enum AstTag
{
   Undefined,
   Statements, While, If, FunctionCall, Assignment,
   LogicalOr, LogicalAnd, Condition, UnaryPlus, UnaryMinus, UnaryNot, BinExpresion,
   Identifier, InterpolatedString,
   Number, Boolean, Function
};

peglib::peg& get_parser();

// vim: et ts=4 sw=4 cin cino={1s ff=unix
