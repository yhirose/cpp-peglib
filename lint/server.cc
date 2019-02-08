#include "httplib.h"
#include "peglib.h"
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>

using namespace httplib;
using namespace std;

static string indexHTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
<title>PEG Playground</title>
<style>
* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    text-decoration: none;
    list-style: none;
}
body {
    display: flex;
    display: -webkit-flex;
    flex-direction: column;
    -webkit-flex-direction: column;
    height: 100vh;
}
#main {
    flex: 1;
    -webkit-flex: 1;
    display: none;
}
.editor-container {
    flex: 1;
    -webkit-flex: 1;
    width: 100%;
    display: flex;
    display: -webkit-flex;
    flex-direction: column;
    -webkit-flex-direction: column;
    margin: 8px;
}
.editor-container:first-child {
    margin-right: 0;
}
.editor-header {
    display: flex;
    display: -webkit-flex;
    height: 48px;
    padding: 4px 8px;
}
.editor-header > li:last-child {
    margin-left: auto;
}
.editor-header > li > span {
    height: 38px;
    line-height: 38px;
}
.editor-header > li > a {
    height: 38px;
    line-height: 38px;
    padding: .3em .5em;
    border: 1px solid red;
}
.editor-validation {
    padding: 9px 11px;
    color: green;
    background-color: lightgreen;
    border-radius: 5px;
}
.editor-validation-invalid {
    color: red;
    background-color: pink;
}
.editor-area {
    flex: 1;
    -webkit-flex: 1;
    border: 1px solid lightgray;
}
.editor-info {
    margin-top: 6px;
    height: 160px;
    border: 1px solid lightgray;
    padding: 8px;
}
.editor-info li {
    cursor: pointer;
}
</style>
</head>
<body>

<div id="main">
    <div class="editor-container">
        <ul class="editor-header">
            <li><span>Grammar:</span></li>
            <li><span id="grammar-validation" class="editor-validation">Valid</span></li>
        </ul>
        <pre id="grammar-editor" class="editor-area">{{syntax}}</pre>
        <div id="grammar-info" class="editor-info"></div>
    </div>
    <div class="editor-container">
        <ul class="editor-header">
            <li><span>Code:</span></li>
            <li><span id="code-validation" class="editor-validation">Valid</span></li>
        </ul>
        <pre id="code-editor" class="editor-area">{{source}}</pre>
        <pre id="code-ast" class="editor-area"></pre>
        <pre id="code-ast-optimized" class="editor-area"></pre>
        <div id="code-info" class="editor-info"></div>
    </div>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.1.9/ace.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/2.1.4/jquery.min.js"></script>
<script>
	// Setup editros
    var grammar = ace.edit("grammar-editor");
    grammar.setShowPrintMargin(false);
    grammar.setValue(localStorage.getItem('grammarText'));
    grammar.moveCursorTo(0, 0);

    var code = ace.edit("code-editor");
    code.setShowPrintMargin(false);
    code.setValue(localStorage.getItem('codeText'));
    code.moveCursorTo(0, 0);

    var codeAst = ace.edit("code-ast");
    codeAst.setShowPrintMargin(false);
    codeAst.setOptions({
        readOnly: true,
        highlightActiveLine: false,
        highlightGutterLine: false
    })
    codeAst.renderer.$cursorLayer.element.style.opacity=0;

    var codeAstOptimized = ace.edit("code-ast-optimized");
    codeAstOptimized.setShowPrintMargin(false);
    codeAstOptimized.setOptions({
        readOnly: true,
        highlightActiveLine: false,
        highlightGutterLine: false
    })
    codeAstOptimized.renderer.$cursorLayer.element.style.opacity=0;

    var generateErrorListHTML = function (errors) {
        var html = '<ul>';

        html += $.map(errors, function (x) {
            return '<li data-ln="' + x.ln + '" data-col="' + x.col + '"><span>' + x.ln + ':' + x.col + '</span> <span>' + x.msg + '</span></li>';
        }).join('');

        html += '<ul>';

        return html;
    };

    var parse = function () {
        var $grammarValidation = $('#grammar-validation');
        var $grammarInfo = $('#grammar-info');
        var grammarText = grammar.getValue();

        var $codeValidation = $('#code-validation');
        var $codeInfo = $('#code-info');
        var codeText = code.getValue();

        localStorage.setItem('grammarText', grammarText);
        localStorage.setItem('codeText', codeText);

        $grammarInfo.html('');
        $grammarValidation.hide();
	    $codeInfo.html('');
        $codeValidation.hide();
        codeAst.setValue('');
        codeAstOptimized.setValue('');

        if (grammarText.length === 0) {
           return;
        }

        $.post("/parse", {
            grammar: grammarText,
            code: codeText
        }).done(function (data) {
            var isValid = data.grammar.length === 0;
            if (isValid) {
                $grammarValidation.removeClass('editor-validation-invalid').text('Valid').show();

                var isValid = data.code.length === 0;
	            if (isValid) {
	                codeAst.insert(data.ast);
	                codeAstOptimized.insert(data.astOptimized);
	                $codeValidation.removeClass('editor-validation-invalid').text('Valid').show();
                } else {
				    var html = generateErrorListHTML(data.code);
	                $codeInfo.html(html);
	                $codeValidation.addClass('editor-validation-invalid').text('Invalid').show();
                }
            } else {
			    var html = generateErrorListHTML(data.grammar);
                $grammarInfo.html(html);
                $grammarValidation.addClass('editor-validation-invalid').text('Invalid').show();
            }
        });
    };

    // Event handing for text editiing
    var timer;
    var setupTimer = function () {
        clearTimeout(timer);
        timer = setTimeout(parse, 750);
    };
    grammar.getSession().on('change', setupTimer);
    code.getSession().on('change', setupTimer);

    // Event handing in the info area
    var makeOnClickInInfo = function (editor) {
    	return function () {
	        var el = $(this);
	        editor.navigateTo(el.data('ln') - 1, el.data('col') - 1);
	        editor.focus();
    	}
    };
    $('#grammar-info').on('click', 'li', makeOnClickInInfo(grammar));
    $('#code-info').on('click', 'li', makeOnClickInInfo(code));

    // Show page
    $('#main').css({
        'display': 'flex',
        'display': '-webkit-flex'
    });

    // Initial parse
    parse();
</script>
</body>
</html>
)";

// https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784
std::string escape_json(const std::string &s) {
    std::ostringstream o;
    for (auto c : s) {
        if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) {
            o << "\\u"
                << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        } else {
            o << c;
        }
    }
    return o.str();
}

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
        json += R"("msg":")" + escape_json(msg) + R"(")";
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

string replace_all(const string& str, const char* from, const char* to)
{
    string ret;
    ret.reserve(str.length());

    size_t from_len = 0;
    while (from[from_len]) {
        from_len++;
    }

    size_t start_pos = 0, pos;
    while ((pos = str.find(from, start_pos)) != string::npos) {
        ret += str.substr(start_pos, pos - start_pos);
        ret += to;
        pos += from_len;
        start_pos = pos;
    }
    ret += str.substr(start_pos);
    return ret;
}

int run_server(int port, const vector<char>& syntax, const vector<char>& source)
{
    Server svr;

    svr.Get("/", [&](const Request& /*req*/, Response& res) {
        indexHTML = replace_all(indexHTML, "{{syntax}}", string(syntax.data(), syntax.size()).c_str());
        indexHTML = replace_all(indexHTML, "{{source}}", string(source.data(), source.size()).c_str());

        res.set_content(indexHTML, "text/html");
    });

    svr.Post("/parse", [](const Request& req, Response& res) {
        const auto& grammarText = req.get_param_value("grammar");

        string grammarResult;
        string codeResult;
        string astResult;
        string astResultOptimized;

        peg::parser peg;
        auto ret = parse_grammar(grammarText, peg, grammarResult);

        if (ret && peg) {
            const auto& codeText = req.get_param_value("code");
            shared_ptr<peg::Ast> ast;
            if (parse_code(codeText, peg, codeResult, ast)) {
                astResult = escape_json(peg::ast_to_s(ast));
                astResultOptimized = escape_json(peg::ast_to_s(peg::AstOptimizer(true).optimize(ast)));
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

        res.set_content(json, "application/json");
    });

    svr.set_error_handler([](const Request& /*req*/, Response& res) {
        const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    cerr << "Server running at http://localhost:" << port << "/" << endl;
    svr.listen("localhost", port);

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
