#include <map>
#include <string>
#include <peglib.h>

struct Environment;

struct Value
{
    enum Type { Undefined, Bool, Long, String, Array, Function };

    struct ArrayValue {
        std::vector<Value> values;
    };

    struct FunctionValue {
        struct Parameter {
            std::string name;
            bool        mut;
        };
        std::vector<Parameter>                                  params;
        std::function<Value (std::shared_ptr<Environment> env)> eval;
    };

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
    explicit Value(ArrayValue&& a) : type(Array), v(a) {}
    explicit Value(FunctionValue&& f) : type(Function), v(f) {}

    bool to_bool() const {
        switch (type) {
            case Bool: return v.get<bool>();
            case Long: return v.get<long>() != 0;
        }
        throw std::runtime_error("type error.");
    }

    long to_long() const {
        switch (type) {
            case Bool: return v.get<bool>();
            case Long: return v.get<long>();
        }
        throw std::runtime_error("type error.");
    }

    std::string to_string() const {
        switch (type) {
            case String: return v.get<std::string>();
        }
        throw std::runtime_error("type error.");
    }

    ArrayValue to_array() const {
        switch (type) {
            case Array: return v.get<ArrayValue>();
        }
        throw std::runtime_error("type error.");
    }

    FunctionValue to_function() const {
        switch (type) {
            case Function: return v.get<FunctionValue>();
        }
        throw std::runtime_error("type error.");
    }

    std::string str() const {
        switch (type) {
            case Undefined: return "undefined";
            case Bool:      return to_bool() ? "true" : "false";
            case Long:      return std::to_string(to_long()); break;
            case String:    return to_string();
            case Function:  return "[function]";
            case Array:     return "[array]";
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

    std::ostream& out(std::ostream& os) const {
        switch (type) {
            case Undefined: os << "undefined"; break;
            case Bool:      os << (to_bool() ? "true" : "false"); break;
            case Long:      os << to_long(); break;
            case String:    os << "'" << to_string() << "'"; break;
            case Function:  os << "[function]"; break;
            case Array:     os << "[array]"; break;
            default: throw std::logic_error("invalid internal condition.");
        }
        return os;
    }

    bool operator==(const Value& rhs) const {
        switch (type) {
            case Undefined: return true; 
            case Bool:      return to_bool() == rhs.to_bool();
            case Long:      return to_long() == rhs.to_long();
            case String:    return to_string() == rhs.to_string();
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
            default: throw std::logic_error("invalid internal condition.");
        }
        // NOTREACHED
    }

private:
    friend std::ostream& operator<<(std::ostream&, const Value&);

    int         type;
    peglib::any v;

};

inline std::ostream& operator<<(std::ostream& os, const Value& val)
{
    return val.out(os);
}

struct Environment
{
    Environment() = default;

    void set_outer(std::shared_ptr<Environment> outer) {
        outer_ = outer;
    }

    bool has(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return true;
        }
        return outer_ && outer_->has(s);
    }

    const Value& get(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return dic_.at(s).val;
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

    void setup_built_in_functions() {
        initialize(
            "pp",
            Value(Value::FunctionValue {
                { {"arg", true} },
                [](std::shared_ptr<Environment> env) {
                    std::cout << env->get("arg").str() << std::endl;
                    return Value();
                }
            }),
            false);
    }

private:
    struct Symbol {
        Value val;
        bool  mut;
    };

    std::shared_ptr<Environment> outer_;
    std::map<std::string, Symbol> dic_;
};

bool run(
    const std::string&           path,
    std::shared_ptr<Environment> env,
    const char*                  expr,
    size_t                       len,
    Value&                       val,
    std::string&                 msg,
    bool                         print_ast);
