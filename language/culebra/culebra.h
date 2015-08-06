#include <map>
#include <sstream>
#include <string>
#include <peglib.h>

namespace culebra {

const auto grammar_ = R"(

    PROGRAM                  <-  _ STATEMENTS
    STATEMENTS               <-  (STATEMENT (';' _)?)*
    STATEMENT                <-  DEBUGGER / RETURN / EXPRESSION

    DEBUGGER                 <-  debugger
    RETURN                   <-  return End _ / return EXPRESSION

    EXPRESSION               <-  ASSIGNMENT / LOGICAL_OR

    ASSIGNMENT               <-  MUTABLE PRIMARY (ARGUMENTS / INDEX / DOT)* '=' _ EXPRESSION

    LOGICAL_OR               <-  LOGICAL_AND ('||' _ LOGICAL_AND)*
    LOGICAL_AND              <-  CONDITION ('&&' _  CONDITION)*
    CONDITION                <-  ADDITIVE (CONDITION_OPERATOR ADDITIVE)*
    ADDITIVE                 <-  UNARY_PLUS (ADDITIVE_OPERATOR UNARY_PLUS)*
    UNARY_PLUS               <-  UNARY_PLUS_OPERATOR? UNARY_MINUS
    UNARY_MINUS              <-  UNARY_MINUS_OPERATOR? UNARY_NOT
    UNARY_NOT                <-  UNARY_NOT_OPERATOR? MULTIPLICATIVE
    MULTIPLICATIVE           <-  CALL (MULTIPLICATIVE_OPERATOR CALL)*

    CALL                     <-  PRIMARY (ARGUMENTS / INDEX / DOT)*
    ARGUMENTS                <-  '(' _ (EXPRESSION (',' _ EXPRESSION)*)? ')' _
    INDEX                    <-  '[' _ EXPRESSION ']' _
    DOT                      <-  '.' _ IDENTIFIER

    WHILE                    <-  while EXPRESSION BLOCK
    IF                       <-  if EXPRESSION BLOCK (else if EXPRESSION BLOCK)* (else BLOCK)?

    PRIMARY                  <-  WHILE / IF / FUNCTION / OBJECT / ARRAY / UNDEFINED / BOOLEAN / NUMBER / IDENTIFIER / STRING / INTERPOLATED_STRING / '(' _ EXPRESSION ')' _

    FUNCTION                 <-  fn PARAMETERS BLOCK
    PARAMETERS               <-  '(' _ (PARAMETER (',' _ PARAMETER)*)? ')' _
    PARAMETER                <-  MUTABLE IDENTIFIER

    BLOCK                    <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR       <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    ADDITIVE_OPERATOR        <-  < [-+] > _
    UNARY_PLUS_OPERATOR      <-  < '+' > _
    UNARY_MINUS_OPERATOR     <-  < '-' > _
    UNARY_NOT_OPERATOR       <-  < '!' > _
    MULTIPLICATIVE_OPERATOR  <-  < [*/%] > _

    IDENTIFIER               <-  < IdentInitChar IdentChar* > _

    OBJECT                   <-  '{' _ (OBJECT_PROPERTY (',' _ OBJECT_PROPERTY)*)? '}' _
    OBJECT_PROPERTY          <-  MUTABLE IDENTIFIER ':' _ EXPRESSION

    ARRAY                    <-  '[' _ (EXPRESSION (',' _ EXPRESSION)*)? ']' _

    UNDEFINED                <-  < 'undefined' > __
    BOOLEAN                  <-  < ('true' / 'false') > __
    MUTABLE                  <-  (< 'mut' > __)?

    ~debugger                <-  'debugger' __
    ~return                  <-  'return' !IdentInitChar Space*
    ~while                   <-  'while' __
    ~if                      <-  'if' __
    ~else                    <-  'else' __
    ~fn                      <-  'fn' __

    NUMBER                   <-  < [0-9]+ > _
    STRING                   <-  ['] < (!['] .)* > ['] _

    INTERPOLATED_STRING      <-  '"' ('{' _ EXPRESSION '}' / INTERPOLATED_CONTENT)* '"' _
    INTERPOLATED_CONTENT     <-  (!["{] .) (!["{] .)*

    ~_                       <-  (Space / End)*
    __                       <-  !IdentInitChar (Space / End)*
    ~Space                   <-  ' ' / '\t' / Comment
    ~End                     <-  EndOfLine / EndOfFile
    Comment                  <-  '/*' (!'*/' .)* '*/' /  ('#' / '//') (!End .)* &End
    EndOfLine                <-  '\r\n' / '\n' / '\r'
    EndOfFile                <-  !.
    IdentInitChar            <-  [a-zA-Z_]
    IdentChar                <-  [a-zA-Z0-9_]

)";

inline peglib::peg& get_parser()
{
    static peglib::peg parser;
    static bool        initialized = false;

    if (!initialized) {
        initialized = true;

        parser.log = [&](size_t ln, size_t col, const std::string& msg) {
            std::cerr << ln << ":" << col << ": " << msg << std::endl;
        };

        if (!parser.load_grammar(grammar_)) {
            throw std::logic_error("invalid peg grammar");
        }

        parser.enable_ast();
    }

    return parser;
}

struct Value;
struct Symbol;
struct Environment;

struct FunctionValue {
    struct Parameter {
        std::string name;
        bool        mut;
    };

    FunctionValue(
        const std::vector<Parameter>& params,
        const std::function<Value (std::shared_ptr<Environment> env)>& eval)
        : params(std::make_shared<std::vector<Parameter>>(params))
        , eval(eval) {}

    std::shared_ptr<std::vector<Parameter>>                 params;
    std::function<Value (std::shared_ptr<Environment> env)> eval;
};

struct ObjectValue {
    bool has(const std::string& name) const;
    const Value& get(const std::string& name) const;
    void assign(const std::string& name, const Value& val);
    void initialize(const std::string& name, const Value& val, bool mut);
    virtual std::map<std::string, Value>& builtins();

    std::shared_ptr<std::map<std::string, Symbol>> properties =
        std::make_shared<std::map<std::string, Symbol>>();
};

struct ArrayValue : public ObjectValue {
    std::map<std::string, Value>& builtins() override;

    std::shared_ptr<std::vector<Value>> values =
        std::make_shared<std::vector<Value>>();
};

struct Value
{
    enum Type { Undefined, Bool, Long, String, Object, Array, Function };

    Value() : type(Undefined) {
        //std::cout << "Val::def ctor: " << std::endl;
    }

    Value(const Value& rhs) : type(rhs.type), v(rhs.v) {
        //std::cout << "Val::copy ctor: " << *this << std::endl;
    }

    Value(Value&& rhs) : type(rhs.type), v(rhs.v) {
        //std::cout << "Val::move ctor: " << *this << std::endl;
    }

    Value& operator=(const Value& rhs) {
        if (this != &rhs) {
            type = rhs.type;
            v = rhs.v;
            //std::cout << "Val::copy=: " << *this << std::endl;
        }
        return *this;
    }

    Value& operator=(Value&& rhs) {
        type = rhs.type;
        v = rhs.v;
        //std::cout << "Val::move=: " << *this << std::endl;
        return *this;
    }

    explicit Value(bool b) : type(Bool), v(b) {}
    explicit Value(long l) : type(Long), v(l) {}
    explicit Value(std::string&& s) : type(String), v(s) {}
    explicit Value(ObjectValue&& o) : type(Object), v(o) {}
    explicit Value(ArrayValue&& a) : type(Array), v(a) {}
    explicit Value(FunctionValue&& f) : type(Function), v(f) {}

    bool to_bool() const {
        switch (type) {
            case Bool: return v.get<bool>();
            case Long: return v.get<long>() != 0;
            default: throw std::runtime_error("type error.");
        }
    }

    long to_long() const {
        switch (type) {
            case Bool: return v.get<bool>();
            case Long: return v.get<long>();
            default: throw std::runtime_error("type error.");
        }
    }

    std::string to_string() const {
        switch (type) {
            case String: return v.get<std::string>();
            default: throw std::runtime_error("type error.");
        }
    }

    FunctionValue to_function() const {
        switch (type) {
            case Function: return v.get<FunctionValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    const ObjectValue& to_object() const {
        switch (type) {
            case Object: return v.get<ObjectValue>();
            case Array: return v.get<ArrayValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    ObjectValue& to_object() {
        switch (type) {
            case Object: return v.get<ObjectValue>();
            case Array: return v.get<ArrayValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    const ArrayValue& to_array() const {
        switch (type) {
            case Array: return v.get<ArrayValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    std::string str_object() const;

    std::string str_array() const {
        const auto& values = *to_array().values;
        std::string s = "[";
        for (auto i = 0u; i < values.size(); i++) {
            if (i != 0) {
                s += ", ";
            }
            s += values[i].str();
        }
        s += "]";
        return s;
    }

    std::string str() const {
        switch (type) {
            case Undefined: return "undefined";
            case Bool:      return to_bool() ? "true" : "false";
            case Long:      return std::to_string(to_long());
            case String:    return "'" + to_string() + "'";
            case Object:    return str_object();
            case Array:     return str_array();
            case Function:  return "[function]";
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    std::ostream& out(std::ostream& os) const {
        os << str();
        return os;
    }

    bool operator==(const Value& rhs) const {
        switch (type) {
            case Undefined: return rhs.type == Undefined;
            case Bool:      return to_bool() == rhs.to_bool();
            case Long:      return to_long() == rhs.to_long();
            case String:    return to_string() == rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    bool operator!=(const Value& rhs) const {
        return !operator==(rhs);
    }

    bool operator<=(const Value& rhs) const {
        switch (type) {
            case Undefined: return false;
            case Bool:      return to_bool() <= rhs.to_bool();
            case Long:      return to_long() <= rhs.to_long();
            case String:    return to_string() <= rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    bool operator<(const Value& rhs) const {
        switch (type) {
            case Undefined: return false;
            case Bool:      return to_bool() < rhs.to_bool();
            case Long:      return to_long() < rhs.to_long();
            case String:    return to_string() < rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    bool operator>=(const Value& rhs) const {
        switch (type) {
            case Undefined: return false;
            case Bool:      return to_bool() >= rhs.to_bool();
            case Long:      return to_long() >= rhs.to_long();
            case String:    return to_string() >= rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    bool operator>(const Value& rhs) const {
        switch (type) {
            case Undefined: return false;
            case Bool:      return to_bool() > rhs.to_bool();
            case Long:      return to_long() > rhs.to_long();
            case String:    return to_string() > rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    Type        type;
    peglib::any v;
};

struct Symbol {
    Value val;
    bool  mut;
};

inline std::ostream& operator<<(std::ostream& os, const Value& val)
{
    return val.out(os);
}

struct Environment
{
    Environment(std::shared_ptr<Environment> parent = nullptr)
        : level(parent ? parent->level + 1 : 0) {
    }

    void append_outer(std::shared_ptr<Environment> outer) {
        if (this->outer) {
            this->outer->append_outer(outer);
        } else {
            this->outer = outer;
        }
    }

    bool has(const std::string& s) const {
        if (dictionary.find(s) != dictionary.end()) {
            return true;
        }
        return outer && outer->has(s);
    }

    Value get(const std::string& s) const {
        if (dictionary.find(s) != dictionary.end()) {
            return dictionary.at(s).val;
        }
        if (outer) {
            return outer->get(s);
        }
        std::string msg = "undefined variable '" + s + "'...";
        throw std::runtime_error(msg);
    }

    void assign(const std::string& s, const Value& val) {
        assert(has(s));
        if (dictionary.find(s) != dictionary.end()) {
            auto& sym = dictionary[s];
            if (!sym.mut) {
                std::string msg = "immutable variable '" + s + "'...";
                throw std::runtime_error(msg);
            }
            //std::cout << "Env::assgin: " << s << std::endl;
            sym.val = val;
            return;
        }
        outer->assign(s, val);
        return;
    }

    void initialize(const std::string& s, const Value& val, bool mut) {
        //std::cout << "Env::initialize: " << s << std::endl;
        dictionary[s] = Symbol{ val, mut };
    }

    size_t                        level;
    std::shared_ptr<Environment>  outer;
    std::map<std::string, Symbol> dictionary;
};

typedef std::function<void (const peglib::Ast& ast, Environment& env, bool force_to_break)> Debugger;

inline bool ObjectValue::has(const std::string& name) const {
    if (properties->find(name) == properties->end()) {
        const auto& props = const_cast<ObjectValue*>(this)->builtins();
        return props.find(name) != props.end();
    }
    return true;
}

inline const Value& ObjectValue::get(const std::string& name) const {
    if (properties->find(name) == properties->end()) {
        const auto& props = const_cast<ObjectValue*>(this)->builtins();
        return props.at(name);
    }
    return properties->at(name).val;
}

inline void ObjectValue::assign(const std::string& name, const Value& val) {
    assert(has(name));
    auto& sym = properties->at(name);
    if (!sym.mut) {
        std::string msg = "immutable property '" + name + "'...";
        throw std::runtime_error(msg);
    }
    sym.val = val;
    return;
}

inline void ObjectValue::initialize(const std::string& name, const Value& val, bool mut) {
    (*properties)[name] = Symbol{ val, mut };
}

inline std::map<std::string, Value>& ObjectValue::builtins() {
    static std::map<std::string, Value> props_ = {
        {
            "size",
            Value(FunctionValue(
                {},
                [](std::shared_ptr<Environment> callEnv) {
                    const auto& val = callEnv->get("this");
                    long n = val.to_object().properties->size();
                    return Value(n);
                }
            ))
        }
    };
    return props_;
}

inline std::map<std::string, Value>& ArrayValue::builtins() {
    static std::map<std::string, Value> props_ = {
        {
            "size",
            Value(FunctionValue(
                {},
                [](std::shared_ptr<Environment> callEnv) {
                    const auto& val = callEnv->get("this");
                    long n = val.to_array().values->size();
                    return Value(n);
                }
            ))
        },
        {
            "push",
            Value(FunctionValue {
                { {"arg", false} },
                [](std::shared_ptr<Environment> callEnv) {
                    const auto& val = callEnv->get("this");
                    const auto& arg = callEnv->get("arg");
                    val.to_array().values->push_back(arg);
                    return Value();
                }
            })
        }
    };
    return props_;
}

inline std::string Value::str_object() const {
    const auto& properties = *to_object().properties;
    std::string s = "{";
    auto it = properties.begin();
    for (; it != properties.end(); ++it) {
        if (it != properties.begin()) {
            s += ", ";
        }
        const auto& name = it->first;
        const auto& sym = it->second;
        if (sym.mut) {
            s += "mut ";
        }
        s += name;
        s += ": ";
        s += sym.val.str();
    }
    s += "}";
    return s;
}

inline void setup_built_in_functions(Environment& env) {
    env.initialize(
        "puts",
        Value(FunctionValue(
            { {"arg", true} },
            [](std::shared_ptr<Environment> env) {
                std::cout << env->get("arg").str() << std::endl;
                return Value();
            }
        )),
        false);

    env.initialize(
        "assert",
        Value(FunctionValue(
            { {"arg", true} },
            [](std::shared_ptr<Environment> env) {
                auto cond = env->get("arg").to_bool();
                if (!cond) {
                    auto line = env->get("__LINE__").to_long();
                    auto column = env->get("__COLUMN__").to_long();
                    std::string msg = "assert failed at " + std::to_string(line) + ":" + std::to_string(column) + ".";
                    throw std::runtime_error(msg);
                }
                return Value();
            }
        )),
        false);
}

struct Interpreter
{
    Interpreter(Debugger debugger = nullptr)
        : debugger_(debugger) {
    }

    Value exec(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        try {
            return eval(ast, env);
        } catch (const Value& val) {
            return val;
        }
    }

private:
    Value eval(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        using peglib::operator"" _;

        if (debugger_) {
            if (ast.original_tag == "STATEMENT"_) {
                auto force_to_break = ast.tag == "DEBUGGER"_;
                debugger_(ast, *env, force_to_break);
            }
        }

        switch (ast.tag) {
            case "STATEMENTS"_:          return eval_statements(ast, env);
            case "WHILE"_:               return eval_while(ast, env);
            case "IF"_:                  return eval_if(ast, env);
            case "FUNCTION"_:            return eval_function(ast, env);
            case "CALL"_:                return eval_call(ast, env);
            case "BLOCK"_:               return eval_block(ast, env);
            case "ASSIGNMENT"_:          return eval_assignment(ast, env);
            case "LOGICAL_OR"_:          return eval_logical_or(ast, env);
            case "LOGICAL_AND"_:         return eval_logical_and(ast, env);
            case "CONDITION"_:           return eval_condition(ast, env);
            case "UNARY_PLUS"_:          return eval_unary_plus(ast, env);
            case "UNARY_MINUS"_:         return eval_unary_minus(ast, env);
            case "UNARY_NOT"_:           return eval_unary_not(ast, env);
            case "ADDITIVE"_:
            case "MULTIPLICATIVE"_:      return eval_bin_expression(ast, env);
            case "IDENTIFIER"_:          return eval_identifier(ast, env);
            case "OBJECT"_:              return eval_object(ast, env);
            case "ARRAY"_:               return eval_array(ast, env);
            case "UNDEFINED"_:           return eval_undefined(ast, env);
            case "BOOLEAN"_:             return eval_bool(ast, env);
            case "NUMBER"_:              return eval_number(ast, env);
            case "INTERPOLATED_STRING"_: return eval_interpolated_string(ast, env);
            case "DEBUGGER"_:            return Value();
            case "RETURN"_:              eval_return(ast, env);
        }

        if (ast.is_token) {
            return Value(std::string(ast.token));
        }

        // NOTREACHED
        throw std::logic_error("invalid Ast type");
    }

    Value eval_statements(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
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

    Value eval_while(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        for (;;) {
            auto cond = eval(*ast.nodes[0], env);
            if (!cond.to_bool()) {
                break;
            }
            eval(*ast.nodes[1], env);
        }
        return Value();
    }

    Value eval_if(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
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

    Value eval_function(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        std::vector<FunctionValue::Parameter> params;
        for (auto node: ast.nodes[0]->nodes) {
            auto mut = node->nodes[0]->token == "mut";
            const auto& name = node->nodes[1]->token;
            params.push_back({ name, mut });
        }

        auto body = ast.nodes[1];

        return Value(FunctionValue(
            params,
            [=](std::shared_ptr<Environment> callEnv) {
                callEnv->append_outer(env);
                return eval(*body, callEnv);
            }
        ));
    };

    Value eval_function_call(const peglib::Ast& ast, std::shared_ptr<Environment> env, const Value& val) {
        const auto& f = val.to_function();
        const auto& params = *f.params;
        const auto& args = ast.nodes;

        if (params.size() <= args.size()) {
            auto callEnv = std::make_shared<Environment>(env);
            callEnv->initialize("self", val, false);
            for (auto iprm = 0u; iprm < params.size(); iprm++) {
                auto param = params[iprm];
                auto arg = args[iprm];
                auto val = eval(*arg, env);
                callEnv->initialize(param.name, val, param.mut);
            }
            callEnv->initialize("__LINE__", Value((long)ast.line), false);
            callEnv->initialize("__COLUMN__", Value((long)ast.column), false);
            try {
                return f.eval(callEnv);
            } catch (const Value& val) {
                return val;
            }
        }

        std::string msg = "arguments error...";
        throw std::runtime_error(msg);
    }

    Value eval_array_reference(const peglib::Ast& ast, std::shared_ptr<Environment> env, const Value& val) {
        const auto& arr = val.to_array();
        auto idx = eval(ast, env).to_long();
        if (0 <= idx && idx < static_cast<long>(arr.values->size())) {
            return arr.values->at(idx);
        } else {
            // TODO: error? or 'undefined'?
        }
        return val;
    }

    Value eval_property(const peglib::Ast& ast, std::shared_ptr<Environment> env, const Value& val) {
        const auto& obj = val.to_object();
        auto name = ast.token;
        if (!obj.has(name)) {
            return Value();
        }
        const auto& prop = obj.get(name);
        if (prop.type == Value::Function) {
            const auto& pf = prop.to_function();
            return Value(FunctionValue(
                *pf.params,
                [=](std::shared_ptr<Environment> callEnv) {
                    callEnv->initialize("this", val, false);
                    return pf.eval(callEnv);
                }
            ));
        }
        return prop;
    }

    Value eval_call(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        using peglib::operator"" _;

        Value val = eval(*ast.nodes[0], env);

        for (auto i = 1u; i < ast.nodes.size(); i++) {
            const auto& postfix = *ast.nodes[i];

            switch (postfix.original_tag) {
                case "ARGUMENTS"_: val = eval_function_call(postfix, env, val);   break;
                case "INDEX"_:     val = eval_array_reference(postfix, env, val); break;
                case "DOT"_:       val = eval_property(postfix, env, val);        break;
                default: throw std::logic_error("invalid internal condition.");
            }
        }

        return std::move(val);
    }

    Value eval_block(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value();
    }

    Value eval_logical_or(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        assert(ast.nodes.size() > 1); // if the size is 1, thes node will be hoisted.
        Value val;
        for (auto node: ast.nodes) {
            val = eval(*node, env);
            if (val.to_bool()) {
                return std::move(val);
            }
        }
        return std::move(val);
    }

    Value eval_logical_and(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        Value val;
        for (auto node: ast.nodes) {
            val = eval(*node, env);
            if (!val.to_bool()) {
                return std::move(val);
            }
        }
        return std::move(val);
    }

    Value eval_condition(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        assert(ast.nodes.size() == 3); // if the size is 1, thes node will be hoisted.

        auto lhs = eval(*ast.nodes[0], env);
        auto ope = eval(*ast.nodes[1], env).to_string();
        auto rhs = eval(*ast.nodes[2], env);

        if (ope == "==") { return Value(lhs == rhs); }
        else if (ope == "!=") { return Value(lhs != rhs); }
        else if (ope == "<=") { return Value(lhs <= rhs); }
        else if (ope == "<") { return Value(lhs < rhs); }
        else if (ope == ">=") { return Value(lhs >= rhs); }
        else if (ope == ">") { return Value(lhs > rhs); }
        else { throw std::logic_error("invalid internal condition."); }
    }

    Value eval_unary_plus(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        assert(ast.nodes.size() == 2); // if the size is 1, thes node will be hoisted.
        return eval(*ast.nodes[1], env);
    }

    Value eval_unary_minus(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        assert(ast.nodes.size() == 2); // if the size is 1, thes node will be hoisted.
        return Value(eval(*ast.nodes[1], env).to_long() * -1);
    }

    Value eval_unary_not(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        assert(ast.nodes.size() == 2); // if the size is 1, thes node will be hoisted.
        return Value(!eval(*ast.nodes[1], env).to_bool());
    }

    Value eval_bin_expression(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        auto ret = eval(*ast.nodes[0], env).to_long();
        for (auto i = 1u; i < ast.nodes.size(); i += 2) {
            auto val = eval(*ast.nodes[i + 1], env).to_long();
            auto ope = eval(*ast.nodes[i], env).to_string()[0];
            switch (ope) {
                case '+': ret += val; break;
                case '-': ret -= val; break;
                case '*': ret *= val; break;
                case '%': ret %= val; break;
                case '/':
                    if (val == 0) {
                        throw std::runtime_error("divide by 0 error");
                    }
                    ret /= val;
                    break;
            }
        }
        return Value(ret);
    }

    bool is_keyword(const std::string& ident)  const {
        static std::set<std::string> keywords = { "undefined", "true", "false", "mut", "debugger", "return", "while", "if", "else", "fn" };
        return keywords.find(ident) != keywords.end();
    }

    Value eval_assignment(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        auto end = ast.nodes.size() - 1;

        auto mut = ast.nodes[0]->token == "mut";
        auto val = eval(*ast.nodes[end], env);

        if (ast.nodes.size() == 3) {
            const auto& ident = ast.nodes[1]->token;
            if (env->has(ident)) {
                env->assign(ident, val);
            } else if (is_keyword(ident)) {
                throw std::runtime_error("left-hand side is invalid variable name.");
            } else {
                env->initialize(ident, val, mut);
            }
            return std::move(val);
        } else {
            using peglib::operator"" _;

            Value lval = eval(*ast.nodes[1], env);

            auto end = ast.nodes.size() - 2;
            for (auto i = 2u; i < end; i++) {
                const auto& postfix = *ast.nodes[i];

                switch (postfix.original_tag) {
                    case "ARGUMENTS"_: lval = eval_function_call(postfix, env, lval);   break;
                    case "INDEX"_:     lval = eval_array_reference(postfix, env, lval); break;
                    case "DOT"_:       lval = eval_property(postfix, env, lval);        break;
                    default: throw std::logic_error("invalid internal condition.");
                }
            }

            const auto& postfix = *ast.nodes[end];

            switch (postfix.original_tag) {
                case "INDEX"_: {
                    const auto& arr = lval.to_array();
                    auto idx = eval(postfix, env).to_long();
                    if (0 <= idx && idx < static_cast<long>(arr.values->size())) {
                        arr.values->at(idx) = val;
                    } else {
                        // TODO: error? or set 'undefined'?
                    }
                    return val;
                }
                case "DOT"_: {
                    auto& obj = lval.to_object();
                    auto name = postfix.token;
                    if (obj.has(name)) {
                        obj.assign(name, val);
                    } else {
                        obj.initialize(name, val, mut);
                    }
                    return val;
                }
                default:
                    throw std::logic_error("invalid internal condition.");
            }
        }
    };

    Value eval_identifier(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return env->get(ast.token);
    };

    Value eval_object(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        ObjectValue obj;
        for (auto i = 0u; i < ast.nodes.size(); i++) {
            const auto& prop = *ast.nodes[i];
            auto mut = prop.nodes[0]->token == "mut";
            const auto& name = prop.nodes[1]->token;
            auto val = eval(*prop.nodes[2], env);
            obj.properties->emplace(name, Symbol{ val, mut });
        }
        return Value(std::move(obj));
    }

    Value eval_array(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        ArrayValue arr;
        for (auto i = 0u; i < ast.nodes.size(); i++) {
            auto expr = ast.nodes[i];
            auto val = eval(*expr, env);
            arr.values->push_back(val);
        }
        return Value(std::move(arr));
    }

    Value eval_undefined(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value();
    };

    Value eval_bool(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value(ast.token == "true");
    };

    Value eval_number(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value(stol(ast.token));
    };

    Value eval_interpolated_string(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        std::string s;
        for (auto node: ast.nodes) {
            const auto& val = eval(*node, env);
            if (val.type == Value::String) {
                s += val.to_string();
            } else {
                s += val.str();
            }
        }
        return Value(std::move(s));
    };

    void eval_return(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.empty()) {
            throw Value();
        } else {
            throw eval(*ast.nodes[0], env);
        }
    }

    Debugger debugger_;
};

inline bool run(
    const std::string&            path,
    std::shared_ptr<Environment>  env,
    const char*                   expr,
    size_t                        len,
    Value&                        val,
    std::vector<std::string>&     msgs,
    std::shared_ptr<peglib::Ast>& ast,
    Debugger                      debugger = nullptr)
{
    try {
        auto& parser = get_parser();

        parser.log = [&](size_t ln, size_t col, const std::string& err_msg) {
            std::stringstream ss;
            ss << path << ":" << ln << ":" << col << ": " << err_msg << std::endl;
            msgs.push_back(ss.str());
        };

        if (parser.parse_n(expr, len, ast, path.c_str())) {
            ast = peglib::AstOptimizer(true, { "PARAMETERS", "ARGUMENTS", "OBJECT", "ARRAY", "RETURN" }).optimize(ast);
            val = Interpreter(debugger).exec(*ast, env);
            return true;
        }
    } catch (std::runtime_error& e) {
        msgs.push_back(e.what());
    }

    return false;
}

} // namespace culebra
