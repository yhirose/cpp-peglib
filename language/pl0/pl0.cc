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

using namespace peg;
using namespace std;

/*
 * PEG Grammar
 */
auto grammar = R"(
    program    <- _ block '.' _

    block      <- const var procedure statement
    const      <- ('CONST' __ ident '=' _ number (',' _ ident '=' _ number)* ';' _)?
    var        <- ('VAR' __ ident (',' _ ident)* ';' _)?
    procedure  <- ('PROCEDURE' __ ident ';' _ block ';' _)*

    statement  <- (assignment / call / statements / if / while / out / in)?
    assignment <- ident ':=' _ expression
    call       <- 'CALL' __ ident
    statements <- 'BEGIN' __ statement (';' _ statement )* 'END' __
    if         <- 'IF' __ condition 'THEN' __ statement
    while      <- 'WHILE' __ condition 'DO' __ statement
    out        <- ('out' __ / 'write' __ / '!' _) expression
    in         <- ('in' __ / 'read' __ / '?' _) ident

    condition  <- odd / compare
    odd        <- 'ODD' __ expression
    compare    <- expression compare_op expression
    compare_op <- < '=' / '#' / '<=' / '<' / '>=' / '>' > _

    expression <- sign term (term_op term)*
    sign       <- < [-+]? > _
    term_op    <- < [-+] > _

    term       <- factor (factor_op factor)*
    factor_op  <- < [*/] > _

    factor     <- ident / number / '(' _ expression ')' _

    ident      <- < [a-z] [a-z0-9]* > _
    number     <- < [0-9]+ > _

    ~_         <- [ \t\r\n]*
    ~__        <- ![a-z0-9_] _
)";

/*
 * Utilities
 */
string format_error_message(const string& path, size_t ln, size_t col, const string& msg) {
    stringstream ss;
    ss << path << ":" << ln << ":" << col << ": " << msg << endl;
    return ss.str();
}

/*
 * Ast
 */
struct SymbolScope;

struct Annotation
{
    shared_ptr<SymbolScope> scope;
};

typedef AstBase<Annotation> AstPL0;

/*
 * Symbol Table
 */
struct SymbolScope
{
    SymbolScope(shared_ptr<SymbolScope> outer) : outer(outer) {}

    bool has_symbol(const string& ident) const {
        auto ret = constants.count(ident) || variables.count(ident);
        return ret ? true : (outer ? outer->has_symbol(ident) : false);
    }

    bool has_constant(const string& ident) const {
        return constants.count(ident) ? true : (outer ? outer->has_constant(ident) : false);
    }

    bool has_variable(const string& ident) const {
        return variables.count(ident) ? true : (outer ? outer->has_variable(ident) : false);
    }

    bool has_procedure(const string& ident) const {
        return procedures.count(ident) ? true : (outer ? outer->has_procedure(ident) : false);
    }

    map<string, int>                constants;
    set<string>                     variables;
    map<string, shared_ptr<AstPL0>> procedures;

private:
    shared_ptr<SymbolScope> outer;
};

void throw_runtime_error(const shared_ptr<AstPL0> node, const string& msg) {
    throw runtime_error(format_error_message(node->path, node->line, node->column, msg));
}

struct SymbolTable
{
    static void build_on_ast(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope = nullptr) {
        switch (ast->tag) {
            case "block"_:      block(ast, scope); break;
            case "assignment"_: assignment(ast, scope); break;
            case "call"_:       call(ast, scope); break;
            case "ident"_:      ident(ast, scope); break;
            default:            for (auto node: ast->nodes) { build_on_ast(node, scope); } break;
        }
    }

private:
    static void block(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> outer) {
        // block <- const var procedure statement
        auto scope = make_shared<SymbolScope>(outer);
        const auto& nodes = ast->nodes;
        constants(nodes[0], scope);
        variables(nodes[1], scope);
        procedures(nodes[2], scope);
        build_on_ast(nodes[3], scope);
        ast->scope = scope;
    }

    static void constants(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        // const <- ('CONST' __ ident '=' _ number(',' _ ident '=' _ number)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            if (scope->has_symbol(ident)) {
                throw_runtime_error(nodes[i], "'" + ident + "' is already defined...");
            }
            auto number = stoi(nodes[i + 1]->token);
            scope->constants.emplace(ident, number);
        }
    }

    static void variables(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        // var <- ('VAR' __ ident(',' _ ident)* ';' _) ?
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 1) {
            const auto& ident = nodes[i]->token;
            if (scope->has_symbol(ident)) {
                throw_runtime_error(nodes[i], "'" + ident + "' is already defined...");
            }
            scope->variables.emplace(ident);
        }
    }

    static void procedures(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        // procedure <- ('PROCEDURE' __ ident ';' _ block ';' _)*
        const auto& nodes = ast->nodes;
        for (auto i = 0u; i < nodes.size(); i += 2) {
            const auto& ident = nodes[i + 0]->token;
            auto block = nodes[i + 1];
            scope->procedures[ident] = block;
            build_on_ast(block, scope);
        }
    }

    static void assignment(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        // assignment <- ident ':=' _ expression
        const auto& ident = ast->nodes[0]->token;
        if (scope->has_constant(ident)) {
            throw_runtime_error(ast->nodes[0], "cannot modify constant value '" + ident + "'...");
        } else if (!scope->has_variable(ident)) {
            throw_runtime_error(ast->nodes[0], "undefined variable '" + ident + "'...");
        }
    }

    static void call(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        // call <- 'CALL' __ ident
        const auto& ident = ast->nodes[0]->token;
        if (!scope->has_procedure(ident)) {
            throw_runtime_error(ast->nodes[0], "undefined procedure '" + ident + "'...");
        }
    }

    static void ident(const shared_ptr<AstPL0> ast, shared_ptr<SymbolScope> scope) {
        const auto& ident = ast->token;
        if (!scope->has_symbol(ident)) {
            throw_runtime_error(ast, "undefined variable '" + ident + "'...");
        }
    }
};

/*
 * Environment
 */
struct Environment
{
    Environment(shared_ptr<SymbolScope> scope, shared_ptr<Environment> outer)
        : scope(scope), outer(outer) {}

    int get_value(const string& ident) const {
        auto it = scope->constants.find(ident);
        if (it != scope->constants.end()) {
            return it->second;
        } else if (scope->variables.count(ident)) {
            return variables.at(ident);
        }
        return outer->get_value(ident);
    }

    void set_variable(const string& ident, int val) {
        if (scope->variables.count(ident)) {
            variables[ident] = val;
        } else {
            outer->set_variable(ident, val);
        }
    }

    shared_ptr<AstPL0> get_procedure(const string& ident) const {
        auto it = scope->procedures.find(ident);
        return it != scope->procedures.end() ? it->second : outer->get_procedure(ident);
    }

private:
    shared_ptr<SymbolScope> scope;
    shared_ptr<Environment> outer;
    map<string, int>        variables;
};

/*
 * Interpreter
 */
struct Interpreter
{
    static void exec(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env = nullptr) {
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
        exec(ast->nodes[3], make_shared<Environment>(ast->scope, outer));
    }

    static void exec_statement(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // statement  <- (assignment / call / statements / if / while / out / in)?
        if (!ast->nodes.empty()) {
            exec(ast->nodes[0], env);
        }
    }

    static void exec_assignment(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // assignment <- ident ':=' _ expression
        env->set_variable(ast->nodes[0]->token, eval(ast->nodes[1], env));
    }

    static void exec_call(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // call <- 'CALL' __ ident
        exec_block(env->get_procedure(ast->nodes[0]->token), env);
    }

    static void exec_statements(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // statements <- 'BEGIN' __ statement (';' _ statement )* 'END' __
        for (auto stmt: ast->nodes) {
            exec(stmt, env);
        }
    }

    static void exec_if(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // if <- 'IF' __ condition 'THEN' __ statement
        if (eval_condition(ast->nodes[0], env)) {
            exec(ast->nodes[1], env);
        }
    }

    static void exec_while(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // while <- 'WHILE' __ condition 'DO' __ statement
        auto cond = ast->nodes[0];
        auto stmt = ast->nodes[1];
        while (eval_condition(cond, env)) {
            exec(stmt, env);
        }
    }

    static void exec_out(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // out <- ('out' __ / 'write' __ / '!' _) expression
        cout << eval(ast->nodes[0], env) << endl;
    }

    static void exec_in(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // in <- ('in' __ / 'read' __ / '?' _) ident
        int val;
        cin >> val;
        env->set_variable(ast->nodes[0]->token, val);
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
        // odd <- 'ODD' __ expression
        return eval_expression(ast->nodes[0], env) != 0;
    }

    static bool eval_compare(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        // compare <- expression compare_op expression
        const auto& nodes = ast->nodes;
        auto lval = eval_expression(nodes[0], env);
        auto op = peg::str2tag(nodes[1]->token.c_str());
        auto rval = eval_expression(nodes[2], env);
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
        const auto& nodes = ast->nodes;
        auto sign = nodes[0]->token;
        auto sign_val = (sign.empty() || sign == "+") ? 1 : -1;
        auto val = eval(nodes[1], env) * sign_val;
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
        const auto& nodes = ast->nodes;
        auto val = eval(nodes[0], env);
        for (auto i = 1u; i < nodes.size(); i += 2) {
            auto ope = nodes[i + 0]->token[0];
            auto rval = eval(nodes[i + 1], env);
            switch (ope) {
                case '*':
                    val = val * rval;
                    break;
                case '/':
                    if (rval == 0) {
                        throw_runtime_error(ast, "divide by 0 error");
                    }
                    val = val / rval;
                    break;
            }
        }
        return val;
    }

    static int eval_ident(const shared_ptr<AstPL0> ast, shared_ptr<Environment> env) {
        return env->get_value(ast->token);
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

    ifstream ifs(path, ios::in | ios::binary);
    if (ifs.fail()) {
        cerr << "can't open the source file." << endl;
        return -1;
    }
    source.resize(static_cast<unsigned int>(ifs.seekg(0, ios::end).tellg()));
    if (!source.empty()) {
        ifs.seekg(0, ios::beg).read(&source[0], static_cast<streamsize>(source.size()));
    }

    // Setup a PEG parser
    parser parser(grammar);
    parser.enable_ast<AstPL0>();
    parser.log = [&](size_t ln, size_t col, const string& msg) {
        cerr << format_error_message(path, ln, col, msg) << endl;
    };

    // Parse the source and make an AST
    shared_ptr<AstPL0> ast;
    if (parser.parse_n(source.data(), source.size(), ast, path)) {
        if (argc > 2 && string("--ast") == argv[2]) {
            print_ast(ast);
        }
        try {
            SymbolTable::build_on_ast(ast);
            Interpreter::exec(ast);
        } catch (const runtime_error& e) {
            cerr << e.what() << endl;
        }
        return 0;
    }

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix

