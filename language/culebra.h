#include <map>
#include <sstream>
#include <string>
#include <peglib.h>

namespace culebra {

const auto grammar_ = R"(

    PROGRAM                  <-  _ STATEMENTS

    STATEMENTS               <-  (EXPRESSION (';' _)?)*

    EXPRESSION               <-  ASSIGNMENT / LOGICAL_OR
    ASSIGNMENT               <-  MUTABLE IDENTIFIER '=' _ EXPRESSION
    WHILE                    <-  'while' _ EXPRESSION BLOCK
    IF                       <-  'if' _ EXPRESSION BLOCK ('else' _ 'if' _ EXPRESSION BLOCK)* ('else' _ BLOCK)?

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

    PRIMARY                  <-  WHILE / IF / FUNCTION / OBJECT / ARRAY / UNDEFINED / BOOLEAN / NUMBER / IDENTIFIER / STRING / INTERPOLATED_STRING / '(' _ EXPRESSION ')' _

    FUNCTION                 <-  'fn' _ PARAMETERS BLOCK
    PARAMETERS               <-  '(' _ (PARAMETER (',' _ PARAMETER)*)? ')' _
    PARAMETER                <-  MUTABLE IDENTIFIER

    BLOCK                    <-  '{' _ STATEMENTS '}' _

    CONDITION_OPERATOR       <-  < ('==' / '!=' / '<=' / '<' / '>=' / '>') > _
    ADDITIVE_OPERATOR        <-  < [-+] > _
    UNARY_PLUS_OPERATOR      <-  < '+' > _
    UNARY_MINUS_OPERATOR     <-  < '-' > _
    UNARY_NOT_OPERATOR       <-  < '!' > _
    MULTIPLICATIVE_OPERATOR  <-  < [*/%] > _

    IDENTIFIER               <-  < [a-zA-Z_][a-zA-Z0-9_]* > _

    OBJECT                   <-  '{' _ (OBJECT_PROPERTY (',' _ OBJECT_PROPERTY)*)? '}' _
    OBJECT_PROPERTY          <-  IDENTIFIER ':' _ EXPRESSION

    ARRAY                    <-  '[' _ (EXPRESSION (',' _ EXPRESSION)*)? ']' _

    UNDEFINED                <-  < 'undefined' > _
    BOOLEAN                  <-  < ('true' / 'false') > _
    NUMBER                   <-  < [0-9]+ > _
    STRING                   <-  ['] < (!['] .)* > ['] _

    INTERPOLATED_STRING      <-  '"' ('{' _ EXPRESSION '}' / INTERPOLATED_CONTENT)* '"' _
    INTERPOLATED_CONTENT     <-  (!["{] .) (!["{] .)*

    MUTABLE                  <-  < 'mut'? > _

    ~_                       <-  (Space / EndOfLine / Comment)*
    Space                    <-  ' ' / '\t'
    EndOfLine                <-  '\r\n' / '\n' / '\r'
    EndOfFile                <-  !.
    Comment                  <-  '/*' (!'*/' .)* '*/' /  ('#' / '//') (!(EndOfLine / EndOfFile) .)* (EndOfLine / EndOfFile)

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

        parser.enable_ast(true, { "PARAMETERS", "ARGUMENTS" });
    }

    return parser;
}

struct Value;
struct Environment;

struct FunctionValue {
    struct Parameter {
        std::string name;
        bool        mut;
    };

    struct Data {
        std::vector<Parameter>                                  params;
        std::function<Value (std::shared_ptr<Environment> env)> eval;
    };

    FunctionValue(
        const std::vector<Parameter>& params,
        const std::function<Value (std::shared_ptr<Environment> env)>& eval) {

        data = std::make_shared<Data>();
        data->params = params;
        data->eval = eval;
    }

    std::shared_ptr<Data> data;
};

struct ObjectValue {
    bool has_property(const std::string& name) const;
    Value get_property(const std::string& name) const;
    virtual std::map<std::string, Value>& builtins();

    std::shared_ptr<std::map<std::string, Value>> properties =
        std::make_shared<std::map<std::string, Value>>();
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

    Type get_type() const {
        return type;
    }

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

    ObjectValue to_object() const {
        switch (type) {
            case Object: return v.get<ObjectValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    ArrayValue to_array() const {
        switch (type) {
            case Array: return v.get<ArrayValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    FunctionValue to_function() const {
        switch (type) {
            case Function: return v.get<FunctionValue>();
            default: throw std::runtime_error("type error.");
        }
    }

    Value get_property(const std::string& name) const {
        switch (type) {
            case Object: return to_object().get_property(name);
            case Array:  return to_array().get_property(name);
            default: throw std::runtime_error("type error.");
        }
    }

    std::string str_object() const {
        const auto& properties = *to_object().properties;
        std::string s = "{";
        auto it = properties.begin();
        for (; it != properties.end(); ++it) {
            if (it != properties.begin()) {
                s += ", ";
            }
            s += '"' + it->first + '"';
            s += ": ";
            s += it->second.str();
        }
        s += "}";
        return s;
    }

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
            case Long:      return std::to_string(to_long()); break;
            case String:    return to_string();
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
        switch (type) {
            case Undefined: return rhs.type != Undefined;
            case Bool:      return to_bool() != rhs.to_bool();
            case Long:      return to_long() != rhs.to_long();
            case String:    return to_string() != rhs.to_string();
            // TODO: Object and Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
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

private:
    friend std::ostream& operator<<(std::ostream&, const Value&);

    Type        type;
    peglib::any v;

};

inline std::ostream& operator<<(std::ostream& os, const Value& val)
{
    return val.out(os);
}

struct Environment
{
    Environment() = default;

    void set_object(const ObjectValue& obj) {
        obj_ = obj;
    }

    void set_outer(std::shared_ptr<Environment> outer) {
        outer_ = outer;
    }

    void append_outer(std::shared_ptr<Environment> outer) {
        if (outer_) {
            outer_->append_outer(outer);
        } else {
            outer_ = outer;
        }
    }

    bool has(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return true;
        }
        if (obj_.has_property(s)) {
            return true;
        }
        return outer_ && outer_->has(s);
    }

    Value get(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return dic_.at(s).val;
        }
        if (obj_.has_property(s)) {
            return obj_.get_property(s);
        }
        if (outer_) {
            return outer_->get(s);
        }
        std::string msg = "undefined variable '" + s + "'...";
        throw std::runtime_error(msg);
    }

    void assign(const std::string& s, const Value& val) {
        if (dic_.find(s) != dic_.end()) {
            auto& sym = dic_[s];
            if (!sym.mut) {
                std::string msg = "immutable variable '" + s + "'...";
                throw std::runtime_error(msg);
            }
            //std::cout << "Env::assgin: " << s << std::endl;
            sym.val = val;
            return;
        }
        if (outer_ && outer_->has(s)) {
            outer_->assign(s, val);
            return;
        }
        // NOTREACHED
        throw std::logic_error("invalid internal condition.");
    }

    void initialize(const std::string& s, const Value& val, bool mut) {
        //std::cout << "Env::initialize: " << s << std::endl;
        dic_[s] = Symbol{val, mut};
    }

private:
    struct Symbol {
        Value val;
        bool  mut;
    };

    std::shared_ptr<Environment>  outer_;
    std::map<std::string, Symbol> dic_;
    ObjectValue                   obj_;
};

inline bool ObjectValue::has_property(const std::string& name) const {
    if (properties->find(name) == properties->end()) {
        const auto& proto = const_cast<ObjectValue*>(this)->builtins();
        return proto.find(name) != proto.end();
    }
    return true;
}

inline Value ObjectValue::get_property(const std::string& name) const {
    if (properties->find(name) == properties->end()) {
        const auto& proto = const_cast<ObjectValue*>(this)->builtins();
        return proto.at(name);
    }
    return properties->at(name);
}

inline std::map<std::string, Value>& ObjectValue::builtins() {
    static std::map<std::string, Value> proto_ = {
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
    return proto_;
}

inline std::map<std::string, Value>& ArrayValue::builtins() {
    static std::map<std::string, Value> proto_ = {
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
    return proto_;
}

inline void setup_built_in_functions(Environment& env) {
    {
        auto f = FunctionValue(
            { {"arg", true} },
            [](std::shared_ptr<Environment> env) {
                std::cout << env->get("arg").str() << std::endl;
                return Value();
            }
        );
        env.initialize("puts", Value(std::move(f)), false);
    }

    {
        auto f = FunctionValue(
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
        );
        env.initialize("assert", Value(std::move(f)), false);
    }
}

struct Eval
{
    static Value eval(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        using peglib::operator"" _;

        switch (ast.tag) {
            case "STATEMENTS"_:          return eval_statements(ast, env);
            case "WHILE"_:               return eval_while(ast, env);
            case "IF"_:                  return eval_if(ast, env);
            case "FUNCTION"_:            return eval_function(ast, env);
            case "CALL"_:                return eval_call(ast, env);
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
        }

        if (ast.is_token) {
            return Value(std::string(ast.token));
        }

        // NOTREACHED
        throw std::logic_error("invalid Ast type");
    }

private:
    static Value eval_statements(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
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

    static Value eval_while(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        for (;;) {
            auto cond = eval(*ast.nodes[0], env);
            if (!cond.to_bool()) {
                break;
            }
            eval(*ast.nodes[1], env);
        }
        return Value();
    }

    static Value eval_if(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
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

    static Value eval_function(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        std::vector<FunctionValue::Parameter> params;
        for (auto node: ast.nodes[0]->nodes) {
            auto mut = node->nodes[0]->token == "mut";
            const auto& name = node->nodes[1]->token;
            params.push_back({ name, mut });
        }

        auto body = ast.nodes[1];

        auto f = FunctionValue(
            params,
            [=](std::shared_ptr<Environment> callEnv) {
                callEnv->append_outer(env);
                return eval(*body, callEnv);
            }
        );

        return Value(std::move(f));
    };

    static Value eval_call(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        using peglib::operator"" _;

        Value val = eval(*ast.nodes[0], env);

        for (auto i = 1u; i < ast.nodes.size(); i++) {
            const auto& n = *ast.nodes[i];
            if (n.original_tag == "ARGUMENTS"_) {
                // Function call
                const auto& f = val.to_function();
                const auto& params = f.data->params;
                const auto& args = n.nodes;
                if (params.size() <= args.size()) {
                    auto callEnv = std::make_shared<Environment>();

                    callEnv->initialize("self", val, false);

                    for (auto iprm = 0u; iprm < params.size(); iprm++) {
                        auto param = params[iprm];
                        auto arg = args[iprm];
                        auto val = eval(*arg, env);
                        callEnv->initialize(param.name, val, param.mut);
                    }

                    callEnv->initialize("__LINE__", Value((long)ast.line), false);
                    callEnv->initialize("__COLUMN__", Value((long)ast.column), false);

                    val = f.data->eval(callEnv);
                } else {
                    std::string msg = "arguments error...";
                    throw std::runtime_error(msg);
                }
            } else if (n.original_tag == "INDEX"_) {
                // Array reference
                const auto& arr = val.to_array();
                auto idx = eval(n, env).to_long();
                if (0 <= idx && idx < static_cast<long>(arr.values->size())) {
                    val = arr.values->at(idx);
                }
            } else if (n.original_tag == "DOT"_) {
                // Property
                auto name = n.token;
                auto prop = val.get_property(name);

                if (prop.get_type() == Value::Function) {
                    const auto& pf = prop.to_function();

                    auto f = FunctionValue(
                        pf.data->params,
                        [=](std::shared_ptr<Environment> callEnv) {
                            callEnv->initialize("this", val, false);
                            if (val.get_type() == Value::Object) {
                                callEnv->set_object(val.to_object());
                            }
                            return pf.data->eval(callEnv);
                        }
                    );

                    val = Value(std::move(f));
                } else {
                    val = prop;
                }
            } else {
                throw std::logic_error("invalid internal condition.");
            }
        }

        return val;
    }

    static Value eval_logical_or(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.size() == 1) {
            return eval(*ast.nodes[0], env);
        } else {
            Value ret;
            for (auto node: ast.nodes) {
                ret = eval(*node, env);
                if (ret.to_bool()) {
                    return ret;
                }
            }
            return ret;
        }
    }

    static Value eval_logical_and(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        Value ret;
        for (auto node: ast.nodes) {
            ret = eval(*node, env);
            if (!ret.to_bool()) {
                return ret;
            }
        }
        return ret;
    }

    static Value eval_condition(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.size() == 1) {
            return eval(*ast.nodes[0], env);
        } else {
            auto lhs = eval(*ast.nodes[0], env);
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
            } else {
                throw std::logic_error("invalid internal condition.");
            }
        }
    }

    static Value eval_unary_plus(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.size() == 1) {
            return eval(*ast.nodes[0], env);
        } else {
            return eval(*ast.nodes[1], env);
        }
    }

    static Value eval_unary_minus(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.size() == 1) {
            return eval(*ast.nodes[0], env);
        } else {
            return Value(eval(*ast.nodes[1], env).to_long() * -1);
        }
    }

    static Value eval_unary_not(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        if (ast.nodes.size() == 1) {
            return eval(*ast.nodes[0], env);
        } else {
            return Value(!eval(*ast.nodes[1], env).to_bool());
        }
    }

    static Value eval_bin_expression(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
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

    static Value eval_assignment(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        const auto& mut = ast.nodes[0]->token;
        const auto& var = ast.nodes[1]->token;
        auto val = eval(*ast.nodes[2], env);
        if (env->has(var)) {
            env->assign(var, val);
        } else {
            env->initialize(var, val, mut == "mut");
        }
        return val;
    };

    static Value eval_identifier(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        const auto& var = ast.token;
        return env->get(var);
    };

    static Value eval_object(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        ObjectValue obj;

        for (auto i = 0u; i < ast.nodes.size(); i++) {
            const auto& prop = *ast.nodes[i];
            const auto& name = prop.nodes[0]->token;
            auto val = eval(*prop.nodes[1], env);
            obj.properties->emplace(name, val);
        }

        return Value(std::move(obj));
    }

    static Value eval_array(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        ArrayValue arr;

        for (auto i = 0u; i < ast.nodes.size(); i++) {
            auto expr = ast.nodes[i];
            auto val = eval(*expr, env);
            arr.values->push_back(val);
        }

        return Value(std::move(arr));
    }

    static Value eval_undefined(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value();
    };

    static Value eval_bool(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value(ast.token == "true");
    };

    static Value eval_number(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        return Value(stol(ast.token));
    };

    static Value eval_interpolated_string(const peglib::Ast& ast, std::shared_ptr<Environment> env) {
        std::string s;
        for (auto node: ast.nodes) {
            const auto& val = eval(*node, env);
            s += val.str();
        }
        return Value(std::move(s));
    };
};

inline bool run(
    const std::string&           path,
    std::shared_ptr<Environment> env,
    const char*                  expr,
    size_t                       len,
    Value&                       val,
    std::string&                 msg,
    bool                         print_ast)
{
    try {
        std::shared_ptr<peglib::Ast> ast;

        auto& parser = get_parser();

        parser.log = [&](size_t ln, size_t col, const std::string& err_msg) {
            std::stringstream ss;
            ss << path << ":" << ln << ":" << col << ": " << err_msg << std::endl;
            msg = ss.str();
        };

        if (parser.parse_n(expr, len, ast)) {
            if (print_ast) {
                ast->print();
            }

            val = Eval::eval(*ast, env);
            return true;
        }
    } catch (std::runtime_error& e) {
        msg = e.what();
    }

    return false;
}

} // namespace culebra
