cpp-peglib
==========

C++11 header-only [PEG](http://en.wikipedia.org/wiki/Parsing_expression_grammar) (Parsing Expression Grammars) library.

*cpp-peglib* tries to provide more expressive parsing experience in a simple way. This library depends on only one header file. So, you can start using it right away just by including `peglib.h` in your project.

The PEG syntax is well described on page 2 in the [document](http://pdos.csail.mit.edu/papers/parsing:popl04.pdf).

How to use
----------

What if we want to extract only tag names in brackets from ` [tag1] [tag2] [tag3] [tag4]... `?

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
peglib::peg parser(R"(
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

This action `[&](const char* s, size_t l)` gives a pointer and length of the matched string. 

There are more actions available. Here is a complete list:

```c++
[](const char* s, size_t l, const std::vector<peglib::any>& v, const std::vector<std::string>& n)
[](const char* s, size_t l, const std::vector<peglib::any>& v)
[](const char* s, size_t l)
[](const std::vector<peglib::any>& v, const std::vector<std::string>& n)
[](const std::vector<peglib::any>& v)
[]()
```

`const std::vector<peglib::any>& v` contains semantic values. `peglib::any` class is very similar to [boost::any](http://www.boost.org/doc/libs/1_57_0/doc/html/any.html). You can obtain a value by castning it to the actual type. In order to determine the actual type, you have to check the return value type of the child action for the semantic value.

`const std::vector<std::string>& n` contains definition names of semantic values.

This is a complete code of a simple calculator. It shows how to associate actions to definitions and set/get semantic values.

```c++
#include <peglib.h>
#include <assert.h>

using namespace peglib;
using namespace std;

int main(void) {
    auto syntax = R"(
        # Grammar for Calculator...
        Additive  <- Multitive '+' Additive / Multitive
        Multitive <- Primary '*' Multitive / Primary
        Primary   <- '(' Additive ')' / Number
        Number    <- [0-9]+
    )";

    peg parser(syntax);

    parser["Additive"] = {
        nullptr,                                      // Default action
        [](const vector<any>& v) {
            return v[0].get<int>() + v[1].get<int>(); // 1st choice
        },
        [](const vector<any>& v) { return v[0]; }     // 2nd choice
    };

    parser["Multitive"] = {
        nullptr,                                      // Default action
        [](const vector<any>& v) {
            return v[0].get<int>() * v[1].get<int>(); // 1st choice
        },
        [](const vector<any>& v) { return v[0]; }     // 2nd choice
    };

    parser["Primary"] = [](const vector<any>& v) {
        return v.size() == 1 ? v[0] : v[1];
    };

    parser["Number"] = [](const char* s, size_t l) {
        return stoi(string(s, l), nullptr, 10);
    };

    int val;
    parser.parse("1+2*3", val);

    assert(val == 7);
}
```

Make a parser with parser operators
-----------------------------------

Instead of makeing a parser by parsing PEG syntax text, we can also construct a parser by hand with *parser operators*. Here is an example:

```c++
using namespace peglib;
using namespace std;

vector<string> tags;

Definition ROOT, TAG_NAME, _;
ROOT     <= seq(_, zom(seq(chr('['), TAG_NAME, chr(']'), _)));
TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const char* s, size_t l) {
                tags.push_back(string(s, l));
            };
_        <= zom(cls(" \t"));

auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");
```

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
| dot      | Any character      |

Sample codes
------------

  * [Calculator](https://github.com/yhirose/cpp-peglib/blob/master/example/calc.cc)
  * [Calculator with parser operators](https://github.com/yhirose/cpp-peglib/blob/master/example/calc2.cc)
  * [PEG syntax Lint utility](https://github.com/yhirose/cpp-peglib/blob/master/lint/peglint.cc)

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
