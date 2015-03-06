//
//  calc3.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include <cstdlib>

using namespace peglib;
using namespace std;

//
//  PEG syntax:
//
//      EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
//      TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
//      FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
//      TERM_OPERATOR    <-  < [-+] > _
//      FACTOR_OPERATOR  <-  < [/*] > _
//      NUMBER           <-  < [0-9]+ > _
//      ~_               <-  [ \t\r\n]*
//

template <typename T, typename U, typename F>
static U reduce(T i, T end, U val, F f) {
    if (i == end) {
        return val;
    }
    std::tie(val, i) = f(val, i);
    return reduce(i, end, val, f);
};

struct ast_node
{
    virtual ~ast_node() = default;
    virtual long eval() = 0;
};

struct ast_ope : public ast_node
{
    ast_ope(char ope, shared_ptr<ast_node> left, shared_ptr<ast_node> right)
        : ope_(ope), left_(left), right_(right) {}

    long eval() override {
        switch (ope_) {
            case '+': return left_->eval() + right_->eval();
            case '-': return left_->eval() - right_->eval();
            case '*': return left_->eval() * right_->eval();
            case '/': return left_->eval() / right_->eval();
        }
        assert(false);
        return 0;
    };

    static shared_ptr<ast_node> create(const SemanticValues& sv) {
        assert(!sv.empty());
        return reduce(
            sv.begin() + 1,
            sv.end(),
            sv[0].get<shared_ptr<ast_node>>(),
            [](shared_ptr<ast_node> r, SemanticValues::const_iterator i) {
                auto ope = (i++)->val.get<char>();
                auto nd = (i++)->val.get<shared_ptr<ast_node>>();
                r = make_shared<ast_ope>(ope, r, nd);
                return make_tuple(r, i);
            });
    }

private:
    char                 ope_;
    shared_ptr<ast_node> left_;
    shared_ptr<ast_node> right_;
};

struct ast_num : public ast_node
{
    ast_num(long num) : num_(num) {}

    long eval() override { return num_; };

    static shared_ptr<ast_node> create(const char* s, size_t l) {
        return make_shared<ast_num>(atol(s));
    }

private:
    long num_;
};

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cout << "usage: calc3 [formula]" << endl;
        return 1;
    }

    const char* s = argv[1];

    const char* syntax =
        "  EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*      "
        "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
        "  FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _   "
        "  TERM_OPERATOR    <-  < [-+] > _                        "
        "  FACTOR_OPERATOR  <-  < [/*] > _                        "
        "  NUMBER           <-  < [0-9]+ > _                      "
        "  ~_               <-  [ \t\r\n]*                        "
        ;

    peg parser(syntax);

    parser["EXPRESSION"]      = ast_ope::create;
    parser["TERM"]            = ast_ope::create;
    parser["TERM_OPERATOR"]   = [](const char* s, size_t l) { return *s; };
    parser["FACTOR_OPERATOR"] = [](const char* s, size_t l) { return *s; };
    parser["NUMBER"]          = ast_num::create;

    shared_ptr<ast_node> ast;
    if (parser.parse_with_value(s, ast)) {
        cout << s << " = " << ast->eval() << endl;
        return 0;
    }

    cout << "syntax error..." << endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
