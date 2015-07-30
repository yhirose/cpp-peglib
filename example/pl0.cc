//
//  pl0.cc - PL/0 interpreter (https://en.wikipedia.org/wiki/PL/0)
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <fstream>
#include <iostream>

using namespace peglib;
using namespace std;

auto grammar = R"(
    program    <- _ block '.' _

    block      <- const var procedure statement
    const      <- ('CONST' _ ident '=' _ number (',' _ ident '=' _ number)* ';' _)?
    var        <- ('VAR' _ ident (',' _ ident)* ';' _)?
    procedure  <- ('PROCEDURE' _ ident ';' _ block ';' _)*

    statement  <- (assignment / call / statements / if / while / out / in)?
    assignment <- ident ':=' _ expression
    call       <- 'CALL' _ ident
    statements <- 'BEGIN' _ statement (';' _ statement )* 'END' _
    if         <- 'IF' _ condition 'THEN' _ statement
    while      <- 'WHILE' _ condition 'DO' _ statement
    out        <- ('out' / 'write' / '!') _ expression
    in         <- ('in' / 'read' / '?') _ ident

    condition  <- odd / compare
    odd        <- 'ODD' _ expression
    compare    <- expression compare_op expression
    compare_op <- < '=' / '#' / '<=' / '<' / '>=' / '>' > _

    expression <- sign term (term_op term)*
    sign       <- < [-+]? > _
    term_op    <- < [-+] > _

    term       <- factor (factor_op factor)*
    factor_op  <- < [*/] > _

    factor     <- ident / number / '(' _ expression ')' _

    ident      <- < [a-z] ([a-z] / [0-9])* > _
    number     <- < [0-9]+ > _

    ~_         <- [ \t\r\n]*
)";

struct Environment
{
    Environment(shared_ptr<Environment> outer = nullptr) : outer(outer) {}

    int get_value(const string& ident) const {
        try {
            return get_constant(ident);
        } catch (...) {
            return get_variable(ident);
        }
    }

    void set_variable(const string& ident, int val) {
        if (variables.find(ident) != variables.end()) {
            variables[ident] = val;
        } else if (outer) {
            return outer->set_variable(ident, val);
        } else {
            throw runtime_error("undefined variable");
        }
    }

    int get_constant(const string& ident) const {
        if (constants.find(ident) != constants.end()) {
            return constants.at(ident);
        } else if (outer) {
            return outer->get_constant(ident);
        } else {
            throw runtime_error("undefined constants");
        }
    }

    int get_variable(const string& ident) const {
        if (variables.find(ident) != variables.end()) {
            return variables.at(ident);
        } else if (outer) {
            return outer->get_variable(ident);
        } else {
            throw runtime_error("undefined variable");
        }
    }

    map<string, int>             constants;
    map<string, int>             variables;
    map<string, shared_ptr<Ast>> procedures;
    shared_ptr<Environment>      outer;
};

struct Interpreter
{
    void exec(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        switch (ast->tag) {
            case "block"_:      exec_block(ast, env); break;
            case "statement"_:  exec_statement(ast, env); break;
            case "assignment"_: exec_assignment(ast, env); break;
            case "call"_:       exec_call(ast, env); break;
            case "statements"_: exec_statements(ast, env); break;
            case "if"_:         exec_if(ast, env); break;
            case "while"_:      exec_while(ast, env); break;
            case "out"_:        exec_out(ast, env); break;
            case "in"_:         exec_in(ast, env); break;
            default:            throw logic_error("invalid Ast type");
        }
    }

private:
    void exec_block(const shared_ptr<Ast> ast, shared_ptr<Environment> outer) {
        // block <- const var procedure statement
        auto env = make_shared<Environment>(outer);
        const auto& nodes = ast->nodes;
        exec_constants(nodes[0], env);
        exec_variables(nodes[1], env);
        exec_procedures(nodes[2], env);
        exec(nodes[3], env);
    }

    void exec_constants(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // const <- ('CONST' _ ident '=' _ number(',' _ ident '=' _ number)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            auto number = stoi(nodes[i + 1]->token);
            env->constants[ident] = number;
        }
    }

    void exec_variables(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // var <- ('VAR' _ ident(',' _ ident)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 1) {
            const auto& ident = nodes[i]->token;
            env->variables[ident] = 0;
        }
    }

    void exec_procedures(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // procedure <- ('PROCEDURE' _ ident ';' _ block ';' _)*
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            auto block = nodes[i + 1];
            env->procedures[ident] = block;
        }
    }

    void exec_statement(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // statement  <-(assignment / call / statements / if / while / out / in) ?
        if (!ast->nodes.empty()) {
            exec(ast->nodes[0], env);
        }
    }

    void exec_assignment(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // assignment <- ident ':=' _ expression
        const auto& ident = ast->nodes[0]->token;
        auto val = eval(ast->nodes[1], env);
        env->set_variable(ident, val);
    }

    void exec_call(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // call <- 'CALL' _ ident
        const auto& ident = ast->nodes[0]->token;
        auto proc = env->procedures[ident];
        exec_block(proc, env);
    }

    void exec_statements(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // statements <- 'BEGIN' _ statement (';' _ statement )* 'END' _
        for (auto stmt: ast->nodes) {
            exec(stmt, env);
        }
    }

    void exec_if(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // if <- 'IF' _ condition 'THEN' _ statement
        auto cond = eval_condition(ast->nodes[0], env);
        auto stmt = ast->nodes[1];
        if (cond) {
            exec(stmt, env);
        }
    }

    void exec_while(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // while <- 'WHILE' _ condition 'DO' _ statement
        auto cond = ast->nodes[0];
        auto stmt = ast->nodes[1];
        auto ret = eval_condition(cond, env);
        while (ret) {
            exec(stmt, env);
            ret = eval_condition(cond, env);
        }
    }

    void exec_out(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // out <- '!' _ expression
        auto val = eval(ast->nodes[0], env);
        cout << val << endl;
    }

    void exec_in(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // in <- '?' _ ident
        int val;
        cin >> val;
        const auto& ident = ast->nodes[0]->token;
        env->variables[ident] = val;
    }

    bool eval_condition(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // condition <- odd / compare
        const auto& node = ast->nodes[0];
        switch (node->tag) {
            case "odd"_:     return eval_odd(node, env);
            case "compare"_: return eval_compare(node, env);
            default:         throw logic_error("invalid Ast type");
        }
    }

    bool eval_odd(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // odd <- 'ODD' _ expression
        auto val = eval_expression(ast->nodes[0], env);
        return val != 0;
    }

    bool eval_compare(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // compare <- expression compare_op expression
        auto lval = eval_expression(ast->nodes[0], env);
        auto op = peglib::str2tag(ast->nodes[1]->token.c_str());
        auto rval = eval_expression(ast->nodes[2], env);
        switch (op) {
            case "="_:  return lval == rval;
            case "#"_:  return lval != rval;
            case "<="_: return lval <= rval;
            case "<"_:  return lval < rval;
            case ">="_: return lval >= rval;
            case ">"_:  return lval > rval;
            default:    throw logic_error("invalid operator");
        }
    }

    int eval(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        switch (ast->tag) {
            case "expression"_: return eval_expression(ast, env);
            case "term"_:       return eval_term(ast, env);
            case "ident"_:      return eval_ident(ast, env);
            case "number"_:     return eval_number(ast, env);
            default:            throw logic_error("invalid Ast type");
        }
    }

    int eval_expression(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // expression <- sign term (term_op term)*
        auto sign = ast->nodes[0]->token;
        auto sign_val = (sign.empty() || sign == "+") ? 1 : -1;
        auto val = eval(ast->nodes[1], env) * sign_val;
        const auto& nodes = ast->nodes;
        for (auto i = 2u; i < nodes.size(); i += 2) {
            auto ope = nodes[i + 0]->token[0];
            auto rval = eval(nodes[i + 1], env);
            switch (ope) {
                case '+':  val = val + rval; break;
                case '-':  val = val - rval; break;
            }
        }
        return val;
    }

    int eval_term(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        // term <- factor (factor_op factor)*
        auto val = eval(ast->nodes[0], env);
        const auto& nodes = ast->nodes;
        for (auto i = 1u; i < nodes.size(); i += 2) {
            auto ope = nodes[i + 0]->token[0];
            auto rval = eval(nodes[i + 1], env);
            switch (ope) {
                case '*':
                    val = val * rval;
                    break;
                case '/':
                    if (rval == 0) {
                        throw runtime_error("divide by 0 error");
                    }
                    val = val / rval;
                    break;
            }
        }
        return val;
    }

    int eval_ident(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        return env->get_value(ast->token);
    }

    int eval_number(const shared_ptr<Ast> ast, shared_ptr<Environment> env) {
        return stol(ast->token);
    }
};

bool read_file(const char* path, vector<char>& buff)
{
    ifstream ifs(path, ios::in | ios::binary);
    if (ifs.fail()) {
        return false;
    }
    buff.resize(static_cast<unsigned int>(ifs.seekg(0, ios::end).tellg()));
    if (!buff.empty()) {
        ifs.seekg(0, ios::beg).read(&buff[0], static_cast<streamsize>(buff.size()));
    }
    return true;
}

int main(int argc, const char** argv)
{
    if (argc < 2) {
        cout << "usage: pl0 PATH [--ast]" << endl;
        return 1;
    }

    auto path = argv[1];
    vector<char> source;
    if (!read_file(path, source)) {
        cerr << "can't open the source file." << endl;
        return -1;
    }

    peg parser(grammar);
    parser.enable_ast(false, { "program", "statement", "statements", "term", "factor" });

    shared_ptr<Ast> ast;
    if (parser.parse_n(source.data(), source.size(), ast, path)) {
        if (argc > 2 && string("--ast") == argv[2]) {
            ast->print();
        }
        Interpreter interp;
        auto env = make_shared<Environment>();
        interp.exec(ast, env);
        return 0;
    }

    cout << "syntax error..." << endl;
    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
