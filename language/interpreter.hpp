#include <map>
#include <string>
#include <peglib.h>

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
            case Undefined: return true;
            case Bool:      return to_bool() == rhs.to_bool();
            case Long:      return to_long() == rhs.to_long();
            case String:    return to_string() == rhs.to_string();
            // TODO: Array support
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    bool operator!=(const Value& rhs) const {
        switch (type) {
            case Undefined: return false;
            case Bool:      return to_bool() != rhs.to_bool();
            case Long:      return to_long() != rhs.to_long();
            case String:    return to_string() != rhs.to_string();
            // TODO: Array support
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
            // TODO: Array support
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
            // TODO: Array support
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
            // TODO: Array support
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
            // TODO: Array support
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

bool run(
    const std::string&           path,
    std::shared_ptr<Environment> env,
    const char*                  expr,
    size_t                       len,
    Value&                       val,
    std::string&                 msg,
    bool                         print_ast);
