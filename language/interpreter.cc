#include "interpreter.hpp"
#include "parser.hpp"
#include <sstream>

using namespace peglib;
using namespace std;

struct Eval
{
    static Value eval(const Ast& ast, shared_ptr<Environment> env) {
        switch (ast.type) {
            case Statements:   return eval_statements(ast, env);
            case While:        return eval_while(ast, env);
            case If:           return eval_if(ast, env);
            case Function:     return eval_function(ast, env);
            case FunctionCall: return eval_function_call(ast, env);
            case Assignment:   return eval_assignment(ast, env);
            case Condition:    return eval_condition(ast, env);
            case BinExpresion: return eval_bin_expression(ast, env);
            case Identifier:   return eval_identifier(ast, env);
            case Number:       return eval_number(ast, env);
            case Boolean:      return eval_bool(ast, env);
            case InterpolatedString: return eval_interpolated_string(ast, env);
        }

        if (ast.is_token) {
            return Value(ast.token);
        }

        // NOTREACHED
        throw logic_error("invalid Ast type");
    }

private:
    static Value eval_statements(const Ast& ast, shared_ptr<Environment> env) {
        if (ast.is_token) {
            return eval(ast, env);
        } else if (ast.nodes.empty()) {
            return Value();
        }
        auto it = ast.nodes.begin();
        while (it != ast.nodes.end() - 1) {
            eval(**it, env);
            ++it;
        }
        return eval(**it, env);
    }

    static Value eval_while(const Ast& ast, shared_ptr<Environment> env) {
        for (;;) {
            auto cond = eval(*ast.nodes[0], env);
            if (!cond.to_bool()) {
                break;
            }
            eval(*ast.nodes[1], env);
        }
        return Value();
    }

    static Value eval_if(const Ast& ast, shared_ptr<Environment> env) {
        const auto& nodes = ast.nodes;

        for (auto i = 0u; i < nodes.size(); i += 2) {
            if (i + 1 == nodes.size()) {
                return eval(*nodes[i], env);
            } else {
                auto cond = eval(*nodes[i], env);
                if (cond.to_bool()) {
                    return eval(*nodes[i + 1], env);
                }
            }
        }

        return Value();
    }

    static Value eval_function(const Ast& ast, shared_ptr<Environment> env) {
        vector<string> params;
        for (auto node: ast.nodes[0]->nodes) {
            params.push_back(node->token);
        }

        auto body = ast.nodes[1];

        auto f = Value::FunctionValue {
            params,
            [=](shared_ptr<Environment> callEnv) {
                callEnv->push_outer(env);
                return eval(*body, callEnv);
            }
        };
        return Value(f);
    };

    static Value eval_function_call(const Ast& ast, shared_ptr<Environment> env) {
        const auto& var = ast.nodes[0]->token;
        const auto& args = ast.nodes[1]->nodes;

        const auto& f = dereference_identirier(env, var);
        const auto& fv = f.to_function();

        if (fv.params.size() <= args.size()) {
            auto callEnv = make_shared<Environment>();

            callEnv->set("self", f);

            for (auto i = 0u; i < fv.params.size(); i++) {
                auto var = fv.params[i];
                auto arg = args[i];
                auto val = eval(*arg, env);
                callEnv->set(var, val);
            }

            callEnv->push_outer(env);

            return fv.eval(callEnv);
        }

        string msg = "arguments error in '" + var + "'...";
        throw runtime_error(msg);
    }

    static Value eval_condition(const Ast& ast, shared_ptr<Environment> env) {
        auto lhs = eval(*ast.nodes[0], env);
        if (ast.nodes.size() > 1) {
            auto ope = eval(*ast.nodes[1], env).to_string();
            auto rhs = eval(*ast.nodes[2], env);

            if (ope == "==") {
                return Value(lhs == rhs);
            } else if (ope == "!=") {
                return Value(lhs != rhs);
            } else if (ope == "<=") {
                return Value(lhs <= rhs);
            } else if (ope == "<") {
                return Value(lhs < rhs);
            } else if (ope == ">=") {
                return Value(lhs >= rhs);
            } else if (ope == ">") {
                return Value(lhs > rhs);
            }

            throw std::logic_error("invalid internal condition.");
        }
        return lhs; // Any
    }

    static Value eval_bin_expression(const Ast& ast, shared_ptr<Environment> env) {
        auto ret = eval(*ast.nodes[0], env).to_long();
        for (auto i = 1u; i < ast.nodes.size(); i += 2) {
            auto val = eval(*ast.nodes[i + 1], env).to_long();
            auto ope = eval(*ast.nodes[i], env).to_string()[0];
            switch (ope) {
                case '+': ret += val; break;
                case '-': ret -= val; break;
                case '*': ret *= val; break;
                case '/': ret /= val; break;
                case '%': ret %= val; break;
            }
        }
        return Value(ret);
    }

    static Value eval_assignment(const Ast& ast, shared_ptr<Environment> env) {
        const auto& var = ast.nodes[0]->token;
        auto val = eval(*ast.nodes[1], env);
        env->set(var, val);
        return val;
    };

    static Value eval_identifier(const Ast& ast, shared_ptr<Environment> env) {
        const auto& var = ast.token;
        return dereference_identirier(env, var);
    };

    static Value eval_number(const Ast& ast, shared_ptr<Environment> env) {
        return Value(stol(ast.token));
    };

    static Value eval_bool(const Ast& ast, shared_ptr<Environment> env) {
        return Value(ast.token == "true");
    };

    static Value eval_interpolated_string(const Ast& ast, shared_ptr<Environment> env) {
        string s;
        for (auto node: ast.nodes) {
            const auto& val = eval(*node, env);
            s += val.str();
        }
        return Value(s);
    };

    static Value dereference_identirier(shared_ptr<Environment> env, const string& var) {
        if (!env->has(var)) {
            string msg = "undefined variable '" + var + "'...";
            throw runtime_error(msg);
        }
        return env->get(var);
    };
};

std::ostream& operator<<(std::ostream& os, const Value& val)
{
    return val.out(os);
}

bool run(const string& path, shared_ptr<Environment> env, const char* expr, size_t len, Value& val, std::string& msg, bool print_ast)
{
    try {
        shared_ptr<Ast> ast;

        auto& parser = get_parser();

        parser.log = [&](size_t ln, size_t col, const string& err_msg) {
            stringstream ss;
            ss << path << ":" << ln << ":" << col << ": " << err_msg << endl;
            msg = ss.str();
        };

        if (parser.parse_n(expr, len, ast)) {
            if (print_ast) {
                ast->print();
            }

            val = Eval::eval(*ast, env);
            return true;
        }
    } catch (runtime_error& e) {
        msg = e.what();
    }

    return false;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
