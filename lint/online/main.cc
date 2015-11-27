
#include "httplib.h"
#include "peglib.h"
#include <cstdio>
#include <functional>

using namespace httplib;
using namespace std;

function<void (size_t, size_t, const string&)> makeJSONFormatter(string& json)
{
    auto init = make_shared<bool>(true);

    return [&json, init](size_t ln, size_t col, const string& msg) mutable {
        if (!init) {
            json += ",";
        }
        json += "{";
        json += R"("ln":)" + to_string(ln) + ",";
        json += R"("col":)" + to_string(col) + ",";
        json += R"("msg":")" + msg + R"(")";
        json += "}";
        *init = false;
    };
}

bool parse_grammar(const string& text, peg::parser& peg, string& json)
{
    peg.log = makeJSONFormatter(json);
    json += "[";
    auto ret = peg.load_grammar(text.data(), text.size());
    json += "]";
    return ret;
}

bool parse_code(const string& text, peg::parser& peg, string& json, shared_ptr<peg::Ast>& ast)
{
    peg.enable_ast();
    peg.log = makeJSONFormatter(json);
    json += "[";
    auto ret = peg.parse_n(text.data(), text.size(), ast);
    json += "]";
    return ret;
}

template <typename T>
void dump_ast(const shared_ptr<T>& ptr, string& json, int level = 0)
{
    const auto& ast = *ptr;
    for (auto i = 0; i < level; i++) {
        json += "  ";
    }
    string name;
    if (ast.name == ast.original_name) {
        name = ast.name;
    } else {
        name = ast.original_name + " (" + ast.name + ")";
    }
    if (ast.is_token) {
        json += "- " + name + "(" + ast.token + ")\\n";
    } else {
        json += "+ " + name +"\\n";
    }
    for (auto node : ast.nodes) {
        dump_ast(node, json, level + 1);
    }
}

int main(void)
{
    Server svr;

    if (!svr.set_base_dir("./www")) {
        cerr << "invalid 'www' directory." << endl;
        return -1;
    }

    svr.post("/parse", [](const Request& req, Response& res) {
        const auto& grammarText = req.params.at("grammar");

        string grammarResult;
        string codeResult;
        string astResult;
        string astResultOptimized;

        peg::parser peg;
        auto ret = parse_grammar(grammarText, peg, grammarResult);

        if (ret && peg) {
            const auto& codeText = req.params.at("code");
            shared_ptr<peg::Ast> ast;
            if (parse_code(codeText, peg, codeResult, ast)) {
                dump_ast(ast, astResult);
                dump_ast(peg::AstOptimizer(true).optimize(ast), astResultOptimized);
            }
        }

        string json;
        json += "{";
        json += "\"grammar\":" + grammarResult;
        if (!codeResult.empty()) {
            json += ",\"code\":" + codeResult;
            json += ",\"ast\":\"" + astResult + "\"";
            json += ",\"astOptimized\":\"" + astResultOptimized + "\"";
        }
        json += "}";

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST");

        res.set_content(json, "application/json");
    });

    svr.set_error_handler([](const Request& req, Response& res) {
        const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    svr.listen("localhost", 1234);

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
