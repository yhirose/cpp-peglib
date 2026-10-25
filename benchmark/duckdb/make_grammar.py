#!/usr/bin/env python3
"""Generates a cpp-peglib grammar from DuckDB's PEG grammar, for the DuckDB comparison.

  make_grammar.py [--lookaheads] <inlined_grammar.gram> <output.peg>

The input is src/include/duckdb/parser/peg/inlined_grammar.gram of a DuckDB release, 2.0.0 or later.

DuckDB matches its grammar over the tokens of its tokenizer (src/parser/peg/tokenizer/base_tokenizer.cpp), and
replaces some rules with matchers written in C++ (compiled_grammar.cpp, BuildTerminalRuleOverrides). cpp-peglib
matches characters, so this script writes both into the grammar:

- A literal matches a whole token, ignoring case. A word literal is checked against a word boundary with %word,
  and an operator literal with a lookahead against the end of DuckDB's operator token. Outside lookaheads, only
  terminal rules hold a token boundary, because cpp-peglib builds a rule with one as an AST token and leaves its
  subtrees out.
- The replaced identifier rules are written from the keyword rules of the same grammar, and the tokenizer's
  whitespace, comments, operators, numbers, strings and identifiers become terminal rules.
- Only the rules reachable from Program are kept, as DuckDB builds only those.

With --lookaheads, the rules that are tried at almost every position and fail almost every time (qualified names,
type literals, '*', function calls, aliases with a colon) get a lookahead in front, a necessary condition for the
rule to match. The grammar accepts the same inputs and builds the same trees; a failed alternative costs a short scan
instead of the rule (see LOOKAHEADS). check_grammar.py checks this.

One rule is not written: DollarQuotedString, a dollar-quoted string ($tag$...$tag$). Matching the closing tag
against the opening one would take a backreference, and a capture keeps packrat parsing from breaking the cycles
of the expression rules, so the programs give the rule as a C++ function (dollar_quote.h).

Differences from the tokenizer: a keyword directly followed by '$' or a non-ASCII character is still read as the
keyword, which keeps the word check on its ASCII fast path.
':' is also matched as the first character of '::' and ':=', which DuckDB's tokenizer reads as one token, so
a[1::2] is accepted. A unicode space that Parser::StripUnicodeSpaces leaves after a '$' (in x$y$z, or right after
a '$') is read as a space, where DuckDB reads it as part of a word.
"""

import re
import sys

RULE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)(\(([A-Za-z_][A-Za-z0-9_]*)\))?\s*<-(.*)$")
REFERENCE = re.compile(r"\b[A-Z][A-Za-z0-9_]*\b")
LITERAL_OR_CLASS = re.compile(r"'(?:[^'\\]|\\.)*'|\[(?:[^\]\\]|\\.)*\]")

# Keyword rules of the grammar, by the keyword category of DuckDB's keyword maps (BuildKeywordMaps).
KEYWORD_RULES = {
    "reserved": "ReservedKeyword",
    "unreserved": "UnreservedKeyword",
    "colname": "ColumnNameKeyword",
    "typefunc": "TypeFuncKeyword",
    "typename": "TypeNameKeyword",
    "barelabel": "BareLabelKeyword",
}

# IdentifierMatcher rules, and the categories of the keywords each one takes as a name (GetIdentifierMask).
DEFAULT_MASK = ("unreserved", "colname")
IDENTIFIER_RULES = {
    "Identifier": DEFAULT_MASK,
    "ColumnName": DEFAULT_MASK,
    "IndexName": DEFAULT_MASK,
    "SequenceName": DEFAULT_MASK,
    "CatalogName": DEFAULT_MASK,
    "SchemaName": DEFAULT_MASK,
    "TableName": DEFAULT_MASK,
    "PragmaName": DEFAULT_MASK,
    "SettingName": DEFAULT_MASK,
    "TypeName": ("unreserved", "typename"),
    "FunctionName": ("unreserved", "typefunc"),
    "TableFunctionName": ("unreserved", "typefunc"),
    "BareColLabel": ("barelabel",),
}
# ReservedIdentifierMatcher rules, which take any identifier, keyword or not.
RESERVED_RULES = [
    "ReservedIdentifier",
    "ReservedColumnName",
    "ReservedIndexName",
    "ReservedSchemaName",
    "ReservedTableName",
    "ReservedFunctionName",
    "ReservedTypeName",
    "ReservedKeyword",
    "CopyOptionName",
]
# The rules of SUGGEST_TABLE_NAME, which also take a single-quoted path (SupportsStringLiteral).
PATH_RULES = {"TableName", "ReservedTableName"}
OVERRIDDEN = set(IDENTIFIER_RULES) | set(RESERVED_RULES) | {
    "NumberLiteral", "StringLiteral", "OperatorLiteral", "AnyOperatorLiteral"}

# Tokenizer::CharacterIsOperator, and the characters that keep a run from losing its trailing + and -.
OPERATOR_CHARS = "-+*/%^<>=~!@&|"
SPECIAL_CHARS = "~!@%^&|"
# OperatorMatcher::HasSpecialPrecedence
SPECIAL_OPERATORS = ["=", "<", ">", "~", "->", "<=", ">=", "!=", "==", "<>", "~~", "~*", "!~", "~~*", "~~~",
                     "!~~", "!~*", "!~~*"]
# The code points Parser::StripUnicodeSpaces turns into spaces before tokenizing.
UNICODE_SPACES = [(0x00A0, 0x00A0), (0x2000, 0x200B), (0x202F, 0x202F), (0x205F, 0x2060), (0x3000, 0x3000),
                  (0xFEFF, 0xFEFF)]

WORD_LITERAL = re.compile(r"'([A-Za-z_][A-Za-z0-9_]*)'")
OPERATOR_LITERAL = re.compile("'([" + re.escape(OPERATOR_CHARS) + "]+)'")
# DuckDB's top-level rules, which PROGRAM replaces: EndOfInput is a matcher written in C++, and cpp-peglib rejects
# repeating TopLevelStatement, which can match nothing.
TOP_LEVEL = {
    "Program": " TopLevelStatement*",
    "TopLevelStatement": " Statement? (';'+ / EndOfInput)",
}
PROGRAM = " (Statement? ';'+)* Statement? !."

BACKSLASH = "\\"


def code_point(cp):
    # Six digits, so that cpp-peglib does not read a following character as part of the number.
    return f"{BACKSLASH}u{cp:06X}"


def ranges_text(ranges):
    return "".join(code_point(lo) if lo == hi else f"{code_point(lo)}-{code_point(hi)}" for lo, hi in ranges)


def non_ascii_word_ranges():
    """The non-ASCII code points that are word characters: all but the unicode spaces."""
    ranges, start = [], 0x80
    for lo, hi in UNICODE_SPACES:
        if start < lo:
            ranges.append((start, lo - 1))
        start = hi + 1
    ranges.append((start, 0x10FFFF))
    return ranges


# One literal for each, so that a choice tells by the first byte where one can start.
UNICODE_SPACE = " / ".join(f"'{code_point(cp)}'" for lo, hi in UNICODE_SPACES for cp in range(lo, hi + 1))
NON_ASCII_WORD_CHARS = ranges_text(non_ascii_word_ranges())
NON_ASCII_CHARS = ranges_text([(0x80, 0x10FFFF)])
# Tokenizer::CharacterIsKeyword: a word starts with a letter, '_' or a non-ASCII character, and goes on with
# digits and '$' too.
WORD_START = f"[a-zA-Z_{NON_ASCII_WORD_CHARS}]"
WORD_CHAR = f"[a-zA-Z0-9_${NON_ASCII_WORD_CHARS}]"
QUOTED = "('\"' ('\"\"' / [^\"])+ '\"' / '`' ('``' / [^`])+ '`')"
# The ASCII characters of a word are a class of their own, so that their runs take the ASCII fast path.
PLAIN = f"!([nNxXeEbB] '\\'') {WORD_START} [a-zA-Z0-9_$]* ([{NON_ASCII_WORD_CHARS}] [a-zA-Z0-9_$]*)*"


def operator_boundary(op):
    """The lookahead that `op` is the whole operator token here."""
    end = "!OperatorRunChar" if set(op) & set(SPECIAL_CHARS) else "!OperatorCont"
    return f"&< '{op}' {end} >"


SPECIAL_PRECEDENCE = " / ".join(operator_boundary(op) for op in SPECIAL_OPERATORS)
LEXICAL_RULES = rf"""
# Tokenizer: spaces, unicode spaces and comments are skipped between tokens. The ASCII spaces are a class of their
# own, which is skipped on a fast path, and the rest is a rule, which a repetition does not enter where the next
# byte cannot start it.
%whitespace <- [ \t\n\r\v\f]* OtherSpace*
%word <- [a-zA-Z0-9_]
OtherSpace <- (UnicodeSpace / LineComment / BlockComment) [ \t\n\r\v\f]*
UnicodeSpace <- {UNICODE_SPACE}
LineComment <- '--' (![\n\r] .)*
BlockComment <- '/*' (BlockComment / !'*/' .)* '*/'

# Tokenizer: an operator token is a run of operator characters, cut where a comment starts, and a run without
# any of ~ ! @ % ^ & | does not keep the + and - at its end (PushOperatorToken).
OperatorChar <- [{OPERATOR_CHARS}]
OperatorRunChar <- !('--' / '/*') OperatorChar
OperatorCont <- ([+] / [-] ![-])* ![-+] OperatorRunChar
OperatorToken <- &((![{SPECIAL_CHARS}] OperatorRunChar)* [{SPECIAL_CHARS}]) OperatorRunChar+ /
    OperatorRunChar OperatorCont*
SpecialPrecedenceOperator <- {SPECIAL_PRECEDENCE}
OperatorLiteral <- < !SpecialPrecedenceOperator OperatorToken >
AnyOperatorLiteral <- < OperatorToken >

# Tokenizer and NumberLiteralMatcher: a number token, which is rejected when the token goes on.
NumberLiteral <- < NumberMantissa NumberExponent? !NumberContinuation >
NumberMantissa <- NumberDigits ('.' NumberDigits?)? / '.' NumberDigits
NumberDigits <- [0-9]+ ([_] [0-9]+)*
NumberExponent <- [eE] [+-]? (NumberDigits / !.)
NumberContinuation <- '.' / [eE] [+-]? ([0-9] / !.)

# Tokenizer and StringLiteralMatcher: a string token, joined by the standard strings that start the next lines.
StringLiteral <- < QuotedString (StringLineBreak StandardString)* / &DollarQuoteStart DollarQuotedString >
QuotedString <- [eE] '\'' ('\\' . / '\'\'' / [^'])* '\'' / [nNxXbB]? StandardString
StandardString <- '\'' ('\'\'' / [^'])* '\''
StringLineBreak <- ([ \t\v\f] / UnicodeSpace / LineComment)* [\n\r] ([ \t\n\r\v\f] / UnicodeSpace / LineComment)*

# Tokenizer: '$', a tag that does not start with a digit, and '$' open a dollar-quoted string, which the same three
# close. Any other '$' is a token of its own. Unicode spaces count as tag characters, as StripUnicodeSpaces leaves
# them in a tag.
DollarQuoteStart <- < '$' ([a-zA-Z_{NON_ASCII_CHARS}] [a-zA-Z0-9_]* ([{NON_ASCII_CHARS}] [a-zA-Z0-9_]*)*)? '$' >
"""


# A quoted or an unquoted word, which every identifier rule and keyword rule takes, and so a lookahead can look for.
NAME = f"({QUOTED} / {PLAIN})"
# The lookahead rules, which LOOKAHEADS refers to.
LOOKAHEAD_RULES = {
    "NameAhead": f"< {NAME} >",
    # where a qualified name can start: a name or a path, and a dot
    "QualifierAhead": "(NameAhead / StringLiteral) '.'",
    "StarAhead": "(NameAhead '.')* '*'",
    "FunctionAhead": "NameAhead ('.' NameAhead)* '('",
    "TableFunctionAhead": "'LATERAL'i? NameAhead ('.' NameAhead)* '('",
    "ColonAliasAhead": "(NameAhead / StringLiteral) ':'",
    "TypeLiteralAhead": "'SETOF'i / NameAhead ('.' NameAhead)* (TypeLiteralParens &(TypeLiteralString / 'ARRAY'i / "
                        "'WITHOUT'i / 'WITH'i) / &(TypeLiteralString / TypeLiteralNextWord))",
    # what a string after a type can start with: a string, or the bracket of an array type
    "TypeLiteralString": "StringLiteral / '['",
    # balanced parentheses, which hold names (read whole, as a string can start inside no name), strings and comments
    "TypeLiteralParens": "'(' (LineComment / BlockComment / StringLiteral / NameAhead / [^()'\"`] / "
                         "TypeLiteralParens)* ')'",
    "TypeLiteralNextWord": "'PRECISION'i / 'VARYING'i / 'CHARACTER'i / 'CHAR'i / 'WITHOUT'i / 'WITH'i / 'ARRAY'i / "
                           "Interval",
}
# rule -> lookahead rule. Each rule is one sequence that starts with what the lookahead looks for.
LOOKAHEADS = {rule: "QualifierAhead" for rule in """
    CatalogReservedSchemaTypeName SchemaReservedTypeName CatalogReservedSchemaIdentifier
    SchemaReservedIdentifierOrStringLiteral NestedSchemaTableColumnName CatalogReservedSchemaTableColumnName
    SchemaReservedTableColumnName TableReservedColumnName CatalogReservedSchemaFunctionName
    SchemaReservedFunctionName SchemaReservedTable CatalogReservedSchemaTable SchemaReservedIndex
    CatalogReservedSchemaIndex IdentifierDot""".split()}
LOOKAHEADS.update({
    "TypeLiteral": "TypeLiteralAhead",
    "StarExpression": "StarAhead",
    "FunctionExpression": "FunctionAhead",
    "ColIdExpression": "ColonAliasAhead",
    "TableAliasColon": "ColonAliasAhead",
    "TableFunctionLateralOpt": "TableFunctionAhead",
})


def is_sequence(body):
    """Whether a rule body has no choice outside parentheses."""
    depth = 0
    for c in LITERAL_OR_CLASS.sub(" ", body):
        depth += (c == "(") - (c == ")")
        if c == "/" and depth == 0:
            return False
    return True


def add_lookaheads(out):
    """Puts the lookahead in front of each rule of LOOKAHEADS, and defines the lookahead rules."""
    for rule, ahead in LOOKAHEADS.items():
        if rule not in out:
            raise SystemExit(f"the rule {rule} to put a lookahead in front of is not in the grammar")
        parameter, body = out[rule]
        if not is_sequence(body):
            raise SystemExit(f"the rule {rule} is not one sequence: {body!r}")
        out[rule] = (parameter, f" &{ahead}{body}")
    for name, body in LOOKAHEAD_RULES.items():
        out[name] = (None, " " + body)


def split_rules(text):
    """Returns the rules in order as [name, parameter, body] and the instruction lines. A body keeps the line
    breaks of its continuation lines; the parameter is None for a rule without one."""
    rules, instructions = [], []
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line.startswith("%"):
            instructions.append(line)
            continue
        m = RULE.match(line)
        if m:
            rules.append([m.group(1), m.group(3), m.group(4)])
        elif rules:
            rules[-1][2] += "\n" + line
        else:
            raise SystemExit(f"text before the first rule: {line!r}")
    return rules, instructions


def references(body, parameter=None):
    return set(REFERENCE.findall(LITERAL_OR_CLASS.sub(" ", body))) - {parameter}


def keyword_words(bodies, name, active=()):
    """PopulateKeywordMap: the literals of a keyword rule, through every rule it refers to."""
    if name in active:
        raise SystemExit(f"keyword rule {name} refers to itself")
    words = {w.upper() for w in re.findall(r"'([^']*)'", bodies[name])}
    for ref in references(bodies[name]):
        words |= keyword_words(bodies, ref, active + (name,))
    return words


def identifier_rule(name, keywords):
    shape = f"({QUOTED} / StandardString / {PLAIN})" if name in PATH_RULES else NAME
    if not keywords:
        return f" < {shape} >"
    choice = " / ".join(f"'{w}'i" for w in sorted(keywords))
    return f" < !(({choice}) !{WORD_CHAR}) {shape} >"


def translate(text, lookaheads=False):
    rules, instructions = split_rules(text)
    if instructions != ["%whitespace <- [ \\t\\n\\r]*"]:
        raise SystemExit(f"unexpected instructions: {instructions!r}")
    bodies = {name: body for name, _, body in rules}
    for name, body in TOP_LEVEL.items():
        if bodies.get(name) != body:
            raise SystemExit(f"the rule {name} is not in the expected form: {bodies.get(name)!r}")
    categories = {c: keyword_words(bodies, r) for c, r in KEYWORD_RULES.items()}
    keywords = set().union(*categories.values())

    # name -> (parameter, body), with Program first, as cpp-peglib starts from the first rule.
    out = {"Program": (None, PROGRAM)}
    for name, parameter, body in rules:
        if name in OVERRIDDEN or name == "Program":
            continue
        # DuckDB matches every literal against a whole token, ignoring case.
        body = WORD_LITERAL.sub(r"'\1'i", body)
        # Parenthesized, so that a suffix such as '?' applies to the lookahead too.
        body = OPERATOR_LITERAL.sub(lambda m: f"({operator_boundary(m.group(1))} '{m.group(1)}')", body)
        body = body.replace("'$'", "(!DollarQuoteStart '$')")
        out[name] = (parameter, body)
    for name, mask in IDENTIFIER_RULES.items():
        allowed = set().union(*(categories[c] for c in mask))
        out[name] = (None, identifier_rule(name, keywords - allowed))
    for name in RESERVED_RULES:
        out[name] = (None, identifier_rule(name, set()))
    if lookaheads:
        add_lookaheads(out)
    lexical = {name for name, _, _ in split_rules(LEXICAL_RULES)[0]}

    # The rules reachable from Program, which are the rules DuckDB builds. The lexical rules are all written.
    reachable, pending = set(), ["Program"]
    while pending:
        name = pending.pop()
        if name in reachable or name in lexical:
            continue
        if name not in out:
            raise SystemExit(f"the rule {name} is referred to but not defined")
        reachable.add(name)
        parameter, body = out[name]
        pending.extend(references(body, parameter))
    lines = []
    for name, (parameter, body) in out.items():
        if name in reachable:
            head = f"{name}({parameter})" if parameter else name
            lines.append(f"{head} <-{body}")
    return "\n".join(lines) + "\n" + LEXICAL_RULES


def main(argv):
    lookaheads = "--lookaheads" in argv
    argv = [a for a in argv if a != "--lookaheads"]
    if len(argv) != 3:
        raise SystemExit(__doc__)
    with open(argv[1], encoding="utf-8") as f:
        text = f.read()
    with open(argv[2], "w", encoding="utf-8") as f:
        f.write(translate(text, lookaheads))


if __name__ == "__main__":
    main(sys.argv)
