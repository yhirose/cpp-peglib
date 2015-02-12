cpp-peglib
==========

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience than common regular expression libraries such as std::regex. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://pdos.csail.mit.edu/papers/parsing:popl04.pdf).

How to use
----------

What if we want to extract only tag names in brackets from ` [tag1] [tag2] [tag3] [tag4]... `? It's a bit hard to do it with *std::regex*, since it doesn't support [Repeated Captures](http://www.boost.org/doc/libs/1_57_0/libs/regex/doc/html/boost_regex/captures.html#boost_regex.captures.repeated_captures). PEG can, however, handle the repetition pretty easily.

PEG grammar for this task could be like this:

```
ROOT      <-  _ ('[' TAG_NAME ']' _)*
TAG_NAME  <-  (!']' .)+
_         <-  [ \t]*
```

Here is how to parse text with the PEG syntax and retrieve tag names:


```c++
// (1) Include the header file
#include "peglib.h"

// (2) Make a parser
auto parser = peglib::make_parser(R"(
    ROOT      <-  _ ('[' TAG_NAME ']' _)*
    TAG_NAME  <-  (!']' .)+
    _         <-  [ \t]*
)");

// (3) Setup an action
std::vector<std::string> tags;
parser["TAG_NAME"] = [&](const char* s, size_t l) {
    tags.push_back(std::string(s, l));
};

// (4) Parse
auto ret = parser.parse(" [tag1] [tag:2] [tag-3] ");

assert(ret     == true);
assert(tags[0] == "tag1");
assert(tags[1] == "tag:2");
assert(tags[2] == "tag-3");
```

You may have a question regarding '(3) Setup an action'. When the parser recognizes the definition 'TAG_NAME', it calls back the action `[&](const char* s, size_t l)` where `const char* s, size_t l` refers to the matched string, so that the user could use the string for something else.

We can do more with actions. A more complex example is here:

```c++
// Calculator example
using namespace peglib;
using namespace std;

auto parser = make_parser(R"(
    # Grammar for Calculator...
    EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
    TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
    FACTOR           <-  NUMBER / '(' EXPRESSION ')'
    TERM_OPERATOR    <-  [-+]
    FACTOR_OPERATOR  <-  [/*]
    NUMBER           <-  [0-9]+
)");

auto reduce = [](const vector<Any>& v) -> long {
    long ret = v[0].get<long>();
    for (auto i = 1u; i < v.size(); i += 2) {
        auto num = v[i + 1].get<long>();
        switch (v[i].get<char>()) {
            case '+': ret += num; break;
            case '-': ret -= num; break;
            case '*': ret *= num; break;
            case '/': ret /= num; break;
        }
    }
    return ret;
};

parser["EXPRESSION"]      = reduce;
parser["TERM"]            = reduce;
parser["TERM_OPERATOR"]   = [](const char* s, size_t l) { return (char)*s; };
parser["FACTOR_OPERATOR"] = [](const char* s, size_t l) { return (char)*s; };
parser["NUMBER"]          = [](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };

long val;
auto ret = parser.parse("1+2*3*(4-5+6)/7-8", val);

assert(ret == true);
assert(val == -3);
```

It may be helpful to keep in mind that the action behavior is similar to the YACC semantic action model ($$, $1, $2, ...).

In this example, the actions return values. These samentic values will be pushed up to the parent definition which can be referred to in the parent action `[](const vector<Any>& v)`. In other words, when a certain definition has been accepted, we can find all semantic values which are associated with the child definitions in `const vector<Any>& v`. The values are wrapped by peglib::Any class which is like `boost::any`. We can retrieve the value by using `get<T>` method where `T` is the actual type of the value. If no value is returned in an action, an undefined `Any` will be pushed up to the parent. Finally, the resulting value of the root definition is received in the out parameter of `parse` method in the parser. `long val` is the resulting value in this case.

Here are available user actions:

```c++
[](const char* s, size_t l, const std::vector<peglib::Any>& v, const std::vector<std::string>& n)
[](const char* s, size_t l, const std::vector<peglib::Any>& v)
[](const char* s, size_t l)
[](const std::vector<peglib::Any>& v, const std::vector<std::string>& n)
[](const std::vector<peglib::Any>& v)
[]()
```

`const std::vector<std::string>& n` holds names of child definitions that could be helpful when we want to check what are the actual child definitions.

Make a parser with parser operators
-----------------------------------

Instead of makeing a parser by parsing PEG syntax text, we can also construct a parser by hand with *parser operators*. Here is an example:

```c++
using namespace peglib;
using namespace std;

vector<string> tags;

Definition ROOT, TAG_NAME, _;
ROOT     = seq(_, zom(seq(chr('['), TAG_NAME, chr(']'), _)));
TAG_NAME = oom(seq(npd(chr(']')), any())), [&](const char* s, size_t l) { tags.push_back(string(s, l)); };
_        = zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

In fact, the PEG parser generator is made with the parser operators. You can see the code at `make_peg_grammar` function in `peglib.h`.

The following are available operators:

| Operator | Description        |
|:---------|:-------------------|
| seq      | Sequence           |
| cho      | Prioritized Choice |
| grp      | Grouping           |
| zom      | Zero or More       |
| oom      | One or More        |
| opt      | Optional           |
| apd      | And predicate      |
| npd      | Not predicate      |
| lit      | Literal string     |
| cls      | Character class    |
| chr      | Character          |
| any      | Any character      |

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator with parser operators](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)

Tested Compilers
----------------

  * Visual Studio 2013
  * Clang 3.5

TODO
----

  * Linear-time parsing (Packrat parsing)
  * Optimization of grammars
  * Unicode support

Other C++ PEG parser libraries
------------------------------

Thanks to the authors of the libraries that inspired *cpp-peglib*.

 * [Boost Spirit X3](https://github.com/djowel/spirit_x3) - A set of C++ libraries for parsing and output generation implemented as Domain Specific Embedded Languages (DSEL) using Expression templates and Template Meta-Programming
 * [PEGTL](https://github.com/ColinH/PEGTL) - Parsing Expression Grammar Template Library
 * [lars::Parser](https://github.com/TheLartians/Parser) - A header-only linear-time c++ parsing expression grammar (PEG) parser generator supporting left-recursion and grammar ambiguity


License
-------

MIT license (© 2015 Yuji Hirose)
