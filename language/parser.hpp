#include <peglib.h>

enum AstTag
{
   Default = peglib::AstDefaultTag,

   Statements,
   While,
   If,
   Call,
   Assignment,

   Arguments,
   Index,
   Dot,

   LogicalOr,
   LogicalAnd,
   Condition,
   UnaryPlus,
   UnaryMinus,
   UnaryNot,
   BinExpresion,

   Identifier,

   Object,
   Array,
   Function,

   InterpolatedString,

   Number,
   Boolean,
};

peglib::peg& get_parser();

// vim: et ts=4 sw=4 cin cino={1s ff=unix
