//
//  pl0.cc - PL/0 interpreter (https://en.wikipedia.org/wiki/PL/0)
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace peglib;
using namespace std;

/*
 * PEG Grammar
 */
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

/*
 * Utilities
 */
string format_error_message(const string& path, size_t ln, size_t col, const string& msg) {
    stringstream ss;
    ss << path << ":" << ln << ":" << col << ": " << msg << endl;
    return ss.str();
}

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

/*
 * Ast
 */
struct Scope;

struct Annotation
{
    shared_ptr<Scope> scope;
};

typedef AstBase<Annotation> AstPL0;

/*
 * Symbol Table
 */
struct Symbol {
    int  value;
    bool is_constant;
};

struct Scope
{
    Scope(shared_ptr<Scope> outer = nullptr) : outer(outer) {}

    bool has_symbol(const string& ident) const {
        auto it = symbols.find(ident);
        if (it != symbols.end()) {
            return true;
        }
        return outer ? outer->has_symbol(ident) : false;
    }

    bool has_constant(const string& ident) const {
        auto it = symbols.find(ident);
        if (it != symbols.end() && it->second.is_constant) {
            return true;
        }
        return outer ? outer->has_constant(ident) : false;
    }

    bool has_variable(const string& ident) const {
        auto it = symbols.find(ident);
        if (it != symbols.end() && !it->second.is_constant) {
            return true;
        }
        return outer ? outer->has_variable(ident) : false;
    }

    bool has_procedure(const string& ident) const {
        auto it = procedures.find(ident);
        if (it != procedures.end()) {
            return true;
        }
        return outer ? outer->has_procedure(ident) : false;
    }

    shared_ptr<AstPL0> get_procedure(const string& ident) const {
        auto it = procedures.find(ident);
        if (it != procedures.end()) {
            return it->second;
        }
        return outer->get_procedure(ident);
    }

    map<string, Symbol>             symbols;
    map<string, shared_ptr<AstPL0>> procedures;

private:
    shared_ptr<Scope> outer;
};

struct SymbolTable
{
    static void build(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope = nullptr) {
        switch (ast->tag) {
            case "block"_:      visit_block(ast, scope); break;
            case "assignment"_: visit_assignment(ast, scope); break;
            case "call"_:       visit_call(ast, scope); break;
            case "ident"_:      visit_ident(ast, scope); break;
            default:            for (auto node: ast->nodes) { build(node, scope); } break;
        }
    }

private:
    static void visit_block(const shared_ptr<AstPL0> ast, shared_ptr<Scope> outer) {
        // block <- const var procedure statement
        auto scope = make_shared<Scope>(outer);
        const auto& nodes = ast->nodes;
        visit_constants(nodes[0], scope);
        visit_variables(nodes[1], scope);
        visit_procedures(nodes[2], scope);
        build(nodes[3], scope);
        ast->scope = scope;
    }

    static void visit_constants(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        // const <- ('CONST' _ ident '=' _ number(',' _ ident '=' _ number)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            auto number = stoi(nodes[i + 1]->token);
            scope->symbols.emplace(ident, Symbol{ number, true });
        }
    }

    static void visit_variables(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        // var <- ('VAR' _ ident(',' _ ident)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 1) {
            const auto& ident = nodes[i]->token;
            scope->symbols.emplace(ident, Symbol{ 0, false });
        }
    }

    static void visit_procedures(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        // procedure <- ('PROCEDURE' _ ident ';' _ block ';' _)*
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            auto block = nodes[i + 1];
            scope->procedures[ident] = block;
            build(block, scope);
        }
    }

    static void visit_assignment(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        // assignment <- ident ':=' _ expression
        const auto& ident = ast->nodes[0]->token;
        if (!scope->has_variable(ident)) {
            string msg = "undefined variable '" + ident + "'...";
            string s = format_error_message(ast->path, ast->line, ast->column, msg);
            throw runtime_error(s);
        }
    }

    static void visit_call(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        // call <- 'CALL' _ ident
        const auto& ident = ast->nodes[0]->token;
        if (!scope->has_procedure(ident)) {
            string msg = "undefined procedure '" + ident + "'...";
            string s = format_error_message(ast->path, ast->line, ast->column, msg);
            throw runtime_error(s);
        }
    }

    static void visit_ident(const shared_ptr<AstPL0> ast, shared_ptr<Scope> scope) {
        const auto& ident = ast->token;
        if (!scope->has_symbol(ident)) {
            string msg = "undefined variable '" + ident + "'...";
            string s = format_error_message(ast->path, ast->line, ast->column, msg);
            throw runtime_error(s);
        }
    }
};

/*
 * Environment
 */
struct Environment
{
    Environment(shared_ptr<Scope> scope = nullptr, shared_ptr<Environment> outer = nullptr) : scope(scope), outer(outer) {
        if (scope) {
            symbols = scope->symbols;
        }
    }

    int get_value(const string& ident) const {
        auto it = symbols.find(ident);
        if (it != symbols.end()) {
            return it->second.value;
        }
        return outer->get_value(ident);
    }

    void set_variable(const string& ident, int val) {
        auto it = symbols.find(ident);
        if (it != symbols.end() && !it->second.is_constant) {
            symbols[ident].value = val;
        } else {
            outer->set_variable(ident, val);
        }
    }

    shared_ptr<Scope> scope;

private:
    map<string, Symbol>     symbols;
    shared_ptr<Environment> outer;
};

/*
 * Interpreter
 */
struct Interpreter
{
    static void exec(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
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
            default:            exec(ast->nodes[0], env); break;
        }
    }

private:
    static void exec_block(const shared_ptr<AstPL0> ast, shared_ptr<Environment> outer) {
        // block <- const var procedure statement
        auto env = make_shared<Environment>(ast->scope, outer);
        exec(ast->nodes[3], env);
    }

    static void exec_statement(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // statement  <-(assignment / call / statements / if / while / out / in) ?
        if (!ast->nodes.empty()) {
            exec(ast->nodes[0], env);
        }
    }

    static void exec_assignment(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // assignment <- ident ':=' _ expression
        const auto& ident = ast->nodes[0]->token;
        auto expr = ast->nodes[1];
        auto val = eval(expr, env);
        env->set_variable(ident, val);
    }

    static void exec_call(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // call <- 'CALL' _ ident
        const auto& ident = ast->nodes[0]->token;
        auto proc = env->scope->get_procedure(ident);
        exec_block(proc, env);
    }

    static void exec_statements(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // statements <- 'BEGIN' _ statement (';' _ statement )* 'END' _
        for (auto stmt: ast->nodes) {
            exec(stmt, env);
        }
    }

    static void exec_if(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // if <- 'IF' _ condition 'THEN' _ statement
        auto cond = eval_condition(ast->nodes[0], env);
        if (cond) {
            auto stmt = ast->nodes[1];
            exec(stmt, env);
        }
    }

    static void exec_while(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // while <- 'WHILE' _ condition 'DO' _ statement
        auto cond = ast->nodes[0];
        auto stmt = ast->nodes[1];
        auto ret = eval_condition(cond, env);
        while (ret) {
            exec(stmt, env);
            ret = eval_condition(cond, env);
        }
    }

    static void exec_out(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // out <- '!' _ expression
        auto val = eval(ast->nodes[0], env);
        cout << val << endl;
    }

    static void exec_in(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // in <- '?' _ ident
        int val;
        cin >> val;
        const auto& ident = ast->nodes[0]->token;
        env->set_variable(ident, val);
    }

    static bool eval_condition(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // condition <- odd / compare
        const auto& node = ast->nodes[0];
        switch (node->tag) {
            case "odd"_:     return eval_odd(node, env);
            case "compare"_: return eval_compare(node, env);
            default:         throw logic_error("invalid AstPL0 type");
        }
    }

    static bool eval_odd(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // odd <- 'ODD' _ expression
        auto val = eval_expression(ast->nodes[0], env);
        return val != 0;
    }

    static bool eval_compare(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
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

    static int eval(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        switch (ast->tag) {
            case "expression"_: return eval_expression(ast, env);
            case "term"_:       return eval_term(ast, env);
            case "ident"_:      return eval_ident(ast, env);
            case "number"_:     return eval_number(ast, env);
            default:            return eval(ast->nodes[0], env);
        }
    }

    static int eval_expression(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
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

    static int eval_term(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
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
                        string msg = "divide by 0 error";
                        throw runtime_error(format_error_message(ast->path, ast->line, ast->column, msg));
                    }
                    val = val / rval;
                    break;
            }
        }
        return val;
    }

    static int eval_ident(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        const auto& ident = ast->token;
        return env->get_value(ident);
    }

    static int eval_number(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        return stol(ast->token);
    }
};

/*
 * Main
 */
int main(int argc, const char** argv)
{
    if (argc < 2) {
        cout << "usage: pl0 PATH [--ast]" << endl;
        return 1;
    }

    // Read a source file into memory
    auto path = argv[1];
    vector<char> source;
    if (!read_file(path, source)) {
        cerr << "can't open the source file." << endl;
        return -1;
    }

    // Setup a PEG parser
    peg parser(grammar);
    parser.enable_ast<AstPL0>();
    parser.log = [&](size_t ln, size_t col, const string& err_msg) {
        cerr << format_error_message(path, ln, col, err_msg) << endl;
    };

    // Parse the source and make an AST
    shared_ptr<AstPL0> ast;
    if (parser.parse_n(source.data(), source.size(), ast, path)) {
        if (argc > 2 && string("--ast") == argv[2]) {
            ast->print();
        }

        try {
            SymbolTable::build(ast);
            Interpreter::exec(ast, make_shared<Environment>());
        } catch (const runtime_error& e) {
            cerr << e.what() << endl;
        }
        return 0;
    }

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
