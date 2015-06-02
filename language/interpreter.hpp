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
        std::vector<std::string>                                params;
        std::function<Value (std::shared_ptr<Environment> env)> eval;
    };

    explicit Value() : type(Undefined) {}
    explicit Value(bool b) : type(Bool), v(b) {}
    explicit Value(long l) : type(Long), v(l) {}
    explicit Value(std::string&& s) : type(String), v(s) {}
    explicit Value(ArrayValue&& a) : type(Array), v(a) {}
    explicit Value(FunctionValue&& f) : type(Function), v(f) {}

    Value(const Value&) = default;
    Value(Value&& rhs) : type(rhs.type), v(rhs.v) {}

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

std::ostream& operator<<(std::ostream& os, const Value& val);

struct Environment
{
    Environment() = default;

    void push_outer(std::shared_ptr<Environment> outer) {
        outers_.push_back(outer);
    }

    bool has(const std::string& s) const {
        if (dic_.find(s) != dic_.end()) {
            return true;
        }
        for (auto& outer: outers_) {
            if (outer->has(s)) {
                return true;
            }
        }
        return false;
    }

    Value get(const std::string& s) const {
        assert(has(s));
        if (dic_.find(s) != dic_.end()) {
            return dic_.at(s);
        }
        for (auto& outer: outers_) {
            if (outer->has(s)) {
                return outer->get(s);
            }
        }
        // NOT REACHED
    }

    void set(const std::string& s, const Value& val) {
        if (dic_.find(s) != dic_.end()) {
            dic_[s] = val;
        }
        for (auto& outer: outers_) {
            if (outer->has(s)) {
                outer->set(s, val);
                return;
            }
        }
        dic_[s] = val;
    }

    void setup_built_in_functions() {
        set("pp", Value(Value::FunctionValue {
            { "arg" },
            [](std::shared_ptr<Environment> env) {
                std::cout << env->get("arg").str() << std::endl;
                return Value();
            }
        }));
    }

private:
    std::vector<std::shared_ptr<Environment>> outers_;
    std::map<std::string, Value> dic_;
};

bool run(const std::string& path, std::shared_ptr<Environment> env, const char* expr, size_t len, Value& val, std::string& msg, bool print_ast);
