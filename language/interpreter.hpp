#include <map>
#include <string>
#include <peglib.h>

struct Env;

struct Value
{
    enum Type { Undefined, Bool, Long, String, Array, Function };

    struct ArrayValue {
        std::vector<Value> values;
    };

    struct FunctionValue {
        std::vector<std::string>        params;
        std::function<Value (Env& env)> eval;
    };

    explicit Value() : type(Undefined) {}
    explicit Value(bool b) : type(Bool) { v = b; }
    explicit Value(long l) : type(Long) { v = l; }
    explicit Value(const std::string& s) : type(String) { v = s; }
    explicit Value(const ArrayValue& a) : type(Array) { v = a; }
    explicit Value(const FunctionValue& f) : type(Function) { v = f; }

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

std::ostream& operator<<(std::ostream& os, const Value& val);

struct Env
{
    Env() : outer_(nullptr) {}
    Env(Env& outer) : outer_(&outer) {}

    bool has(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return true;
        }
        if (outer_) {
            return outer_->has(s);
        }
        return false;
    }

    Value get(const std::string& s) const {
        assert(has(s));
        if (dic_.find(s) != dic_.end()) {
            return dic_.at(s);
        }
        return outer_->get(s);
    }

    void set(const std::string& s, const Value& val) {
        dic_[s] = val;
    }

    void setup_built_in_functions() {
        auto func_print = Value::FunctionValue {
            { "arg" },
            [](Env& env) {
                std::cout << env.get("arg") << std::endl;
                return Value();
            }
        };
        set("print", Value(func_print));
    }

private:
    Env*                         outer_;
    std::map<std::string, Value> dic_;
};

bool run(const std::string& path, Env& env, const char* expr, size_t len, Value& val, std::string& msg, bool print_ast);
