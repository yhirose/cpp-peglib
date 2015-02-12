//
//  peglib.h
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef _CPPEXPATLIB_PEGLIB_H_
#define _CPPEXPATLIB_PEGLIB_H_

#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <cassert>
#include <cstring>
#include <initializer_list>

namespace peglib {

void* enabler;

/*-----------------------------------------------------------------------------
 *  Any
 *---------------------------------------------------------------------------*/

class Any
{
public:
    Any() : content_(nullptr) {}

    Any(const Any& rhs) : content_(rhs.clone()) {}

    Any(Any&& rhs) : content_(rhs.content_) {
        rhs.content_ = nullptr;
    }

    template <typename T>
    Any(const T& value) : content_(new holder<T>(value)) {}

    Any& operator=(const Any& rhs) {
        if (this != &rhs) {
            if (content_) {
                delete content_;
            }
            content_ = rhs.clone();
        }
        return *this;
    }

    Any& operator=(Any&& rhs) {
        if (this != &rhs) {
            if (content_) {
                delete content_;
            }
            content_ = rhs.content_;
            rhs.content_ = nullptr;
        }
        return *this;
    }

    template <typename T>
    Any& operator=(const T& value) {
        if (content_) {
            delete content_;
        }
        content_ = new holder<T>(value);
        return *this;
    }

    ~Any() {
        delete content_;
    }

    bool is_undefined() const {
        return content_ == nullptr;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, Any>::value>::type*& = enabler
    >
    T& get() {
        assert(content_);
        return dynamic_cast<holder<T>*>(content_)->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, Any>::value>::type*& = enabler
    >
    T& get() {
        return *this;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, Any>::value>::type*& = enabler
    >
    const T& get() const {
        assert(content_);
        return dynamic_cast<holder<T>*>(content_)->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, Any>::value>::type*& = enabler
    >
    const Any& get() const {
        return *this;
    }

    // TODO: Add more implecit cast operators
    operator bool() const { return get<bool>(); }
    operator char() const { return get<char>(); }
    operator wchar_t() const { return get<wchar_t>(); }
    operator char16_t() const { return get<char16_t>(); }
    operator char32_t() const { return get<char32_t>(); }
    operator unsigned char() const { return get<unsigned char>(); }
    operator int() const { return get<int>(); }
    operator unsigned int() const { return get<unsigned int>(); }
    operator short() const { return get<short>(); }
    operator unsigned short() const { return get<unsigned short>(); }
    operator long() const { return get<long>(); }
    operator unsigned long() const { return get<unsigned long>(); }
    operator long long() const { return get<long long>(); }
    operator unsigned long long() const { return get<unsigned long long>(); }
    operator float() const { return get<float>(); }
    operator double() const { return get<double>(); }
    operator const std::string&() const { return get<std::string>(); }

private:
    struct placeholder {
        virtual ~placeholder() {};
        virtual placeholder* clone() const = 0;
    };

    template <typename T>
    struct holder : placeholder {
        holder(const T& value) : value_(value) {}
        placeholder* clone() const override {
            return new holder(value_);
        }
        T value_;
    };

    placeholder* clone() const {
        return content_ ? content_->clone() : nullptr;
    }

    placeholder* content_;
};

/*-----------------------------------------------------------------------------
 *  PEG
 *---------------------------------------------------------------------------*/

/*
* Semantic values
*/
struct Values
{
    std::vector<std::string> names;
    std::vector<Any>         values;
};

/*
 * Semantic action
 */
template <
    typename R, typename F,
    typename std::enable_if<!std::is_void<R>::value>::type*& = enabler,
    typename... Args>
Any call(F fn, Args&&... args) {
    return Any(fn(std::forward<Args>(args)...));
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_void<R>::value>::type*& = enabler,
    typename... Args>
Any call(F fn, Args&&... args) {
    fn(std::forward<Args>(args)...);
    return Any();
}

class Action
{
public:
    Action() = default;

    Action(const Action& rhs) : fn_(rhs.fn_) {}

    //Action(Action&& rhs) : fn_(std::move(rhs.fn_)) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_null_pointer<F>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, &F::operator())) {}

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, fn)) {}

    template <typename F, typename std::enable_if<std::is_null_pointer<F>::value>::type*& = enabler>
    Action(F fn) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_null_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, &F::operator());
    }

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, fn);
    }

    template <typename F, typename std::enable_if<std::is_null_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {}

    operator bool() const {
        return (bool)fn_;
    }

    Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) const {
        return fn_(s, l, v, n);
    }

private:
    template <typename R>
    struct TypeAdaptor {
        TypeAdaptor(std::function<R (const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n)> fn)
            : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l, v, n);
        }
        std::function<R (const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l_v {
        TypeAdaptor_s_l_v(std::function<R (const char* s, size_t l, const std::vector<Any>& v)> fn)
            : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l, v);
        }
        std::function<R (const char* s, size_t l, const std::vector<Any>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l {
        TypeAdaptor_s_l(std::function<R (const char* s, size_t l)> fn) : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l);
        }
        std::function<R (const char* s, size_t l)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v_n {
        TypeAdaptor_v_n(std::function<R (const std::vector<Any>& v, const std::vector<std::string>& n)> fn) : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, v, n);
        }
        std::function<R (const std::vector<Any>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v {
        TypeAdaptor_v(std::function<R (const std::vector<Any>& v)> fn) : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, v);
        }
        std::function<R (const std::vector<Any>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_empty {
        TypeAdaptor_empty(std::function<R ()> fn) : fn_(fn) {}
        Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_);
        }
        std::function<R ()> fn_;
    };

    typedef std::function<Any (const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n)> Fty;

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<Any>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<Any>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<Any>& v) const) {
        return TypeAdaptor_s_l_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<Any>& v)) {
        return TypeAdaptor_s_l_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t) const) {
        return TypeAdaptor_s_l<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const char*, size_t)) {
        return TypeAdaptor_s_l<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<Any>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<Any>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<Any>& v) const) {
        return TypeAdaptor_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<Any>& v)) {
        return TypeAdaptor_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)() const) {
        return TypeAdaptor_empty<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)()) {
        return TypeAdaptor_empty<R>(fn);
    }

    Fty fn_;
};

/*
 * Match
 */
struct Match
{
    Match(bool _ret, size_t _len, size_t _id) : ret(_ret), len(_len), id(_id) {}
    bool   ret;
    size_t len;
    size_t id;
};

Match success(size_t len, size_t id = 0) {
    return Match(true, len, id);
}

Match fail() {
    return Match(false, 0, -1);
}

/*
 * Parser operators
 */
class Ope
{
public:
    virtual ~Ope() {};

    virtual Match parse_core(const char* s, size_t l, Values& v) const = 0;

    virtual Match parse(const char* s, size_t l, Values& v) const {
        // NOTE: This is a good place to set a break point for debugging...
        return parse_core(s, l, v);
    }
};

class Sequence : public Ope
{
public:
    Sequence(const Sequence& rhs) : opes_(rhs.opes_) {}

#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    // NOTE: Compiler Error C2797 on Visual Studio 2013
    // "The C++ compiler in Visual Studio does not implement list
    // initialization inside either a member initializer list or a non-static
    // data member initializer. Before Visual Studio 2013 Update 3, this was
    // silently converted to a function call, which could lead to bad code
    // generation. Visual Studio 2013 Update 3 reports this as an error."
    template <typename... Args>
    Sequence(const Args& ...args) {
        opes_ = std::vector<std::shared_ptr<Ope>>{ static_cast<std::shared_ptr<Ope>>(args)... };
    }
#else
    template <typename... Args>
    Sequence(const Args& ...args) : opes_{ static_cast<std::shared_ptr<Ope>>(args)... } {}
#endif

    Sequence(const std::vector<std::shared_ptr<Ope>>& opes) : opes_(opes) {}
    Sequence(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(std::move(opes)) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        size_t i = 0;
        for (const auto& ope : opes_) {
            auto m = ope->parse(s + i, l - i, v);
            if (!m.ret) {
                return fail();
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::vector<std::shared_ptr<Ope>> opes_;
};

class PrioritizedChoice : public Ope
{
public:
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    // NOTE: Compiler Error C2797 on Visual Studio 2013
    // "The C++ compiler in Visual Studio does not implement list
    // initialization inside either a member initializer list or a non-static
    // data member initializer. Before Visual Studio 2013 Update 3, this was
    // silently converted to a function call, which could lead to bad code
    // generation. Visual Studio 2013 Update 3 reports this as an error."
    template <typename... Args>
    PrioritizedChoice(const Args& ...args) {
        opes_ = std::vector<std::shared_ptr<Ope>>{ static_cast<std::shared_ptr<Ope>>(args)... };
    }
#else
    template <typename... Args>
    PrioritizedChoice(const Args& ...args) : opes_{ static_cast<std::shared_ptr<Ope>>(args)... } {}
#endif

    PrioritizedChoice(const std::vector<std::shared_ptr<Ope>>& opes) : opes_(opes) {}
    PrioritizedChoice(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(std::move(opes)) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        size_t id = 0;
        for (const auto& ope : opes_) {
            Values chldsv;
            auto m = ope->parse(s, l, chldsv);
            if (m.ret) {
                if (!chldsv.values.empty()) {
                    for (const auto& x: chldsv.values) {
                        v.values.push_back(x);
                    }
                    for (const auto& x: chldsv.names) {
                        v.names.push_back(x);
                    }
                }
                return success(m.len, id);
            }
            id++;
        }
        return fail();
    }

    size_t size() const { return opes_.size();  }

private:
    std::vector<std::shared_ptr<Ope>> opes_;
};

class ZeroOrMore : public Ope
{
public:
    ZeroOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto i = 0;
        while (l - i > 0) {
            auto m = ope_->parse(s + i, l - i, v);
            if (!m.ret) {
                break;
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::shared_ptr<Ope> ope_;
};

class OneOrMore : public Ope
{
public:
    OneOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto m = ope_->parse(s, l, v);
        if (!m.ret) {
            return fail();
        }
        auto i = m.len;
        while (l - i > 0) {
            auto m = ope_->parse(s + i, l - i, v);
            if (!m.ret) {
                break;
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::shared_ptr<Ope> ope_;
};

class Option : public Ope
{
public:
    Option(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto m = ope_->parse(s, l, v);
        return success(m.ret ? m.len : 0);
    }

private:
    std::shared_ptr<Ope> ope_;
};

class AndPredicate : public Ope
{
public:
    AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto m = ope_->parse(s, l, v);
        if (m.ret) {
            return success(0);
        } else {
            return fail();
        }
    }

private:
    std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope
{
public:
    NotPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto m = ope_->parse(s, l, v);
        if (m.ret) {
            return fail();
        } else {
            return success(0);
        }
    }

private:
    std::shared_ptr<Ope> ope_;
};

class LiteralString : public Ope
{
public:
    LiteralString(const char* s) : lit_(s) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto i = 0u;
        for (; i < lit_.size(); i++) {
            if (i >= l || s[i] != lit_[i]) {
                return fail();
            }
        }
        return success(i);
    }

private:
    std::string lit_;
};

class CharacterClass : public Ope
{
public:
    CharacterClass(const char* chars) : chars_(chars) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1) {
            return fail();
        }
        auto ch = s[0];
        auto i = 0u;
        while (i < chars_.size()) {
            if (i + 2 < chars_.size() && chars_[i + 1] == '-') {
                if (chars_[i] <= ch && ch <= chars_[i + 2]) {
                    return success(1);
                }
                i += 3;
            } else {
                if (chars_[i] == ch) {
                    return success(1);
                }
                i += 1;
            }
        }
        return fail();
    }

private:
    std::string chars_;
};

class Character : public Ope
{
public:
    Character(char ch) : ch_(ch) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1 || s[0] != ch_) {
            return fail();
        }
        return success(1);
    }

private:
    char ch_;
};

class AnyCharacter : public Ope
{
public:
    Match parse_core(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1) {
            return fail();
        }
        return success(1);
    }

};

class Grouping : public Ope
{
public:
    Grouping(const std::shared_ptr<Ope>& ope) : ope_(ope) {}
    Grouping(const std::shared_ptr<Ope>& ope, std::function<void(const char* s, size_t l)> match) : ope_(ope), match_(match) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        assert(ope_);
        auto m = ope_->parse(s, l, v);
        if (m.ret && match_) {
            match_(s, m.len);
        }
        return m;
    }

private:
    std::shared_ptr<Ope>                        ope_;
    std::function<void(const char* s, size_t l)> match_;
};

class WeakHolder : public Ope
{
public:
    WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        auto ope = weak_.lock();
        assert(ope);
        return ope->parse(s, l, v);
    }

private:
    std::weak_ptr<Ope> weak_;
};

/*
 * Definition
 */
class Definition
{
public:
    Definition()
       : actions(1)
       , holder_(std::make_shared<Holder>(this)) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , actions(1)
        , holder_(rhs.holder_)
    {
        holder_->outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , actions(1)
        , holder_(std::move(rhs.holder_))
    {
        holder_->outer_ = this;
    }

    Definition(const std::shared_ptr<Ope>& ope)
        : actions(1)
        , holder_(std::make_shared<Holder>(this))
    {
        holder_->ope_ = ope;
    }

    operator std::shared_ptr<Ope>() {
        return std::make_shared<WeakHolder>(holder_);
    }

    Definition& operator<=(const std::shared_ptr<Ope>& ope) {
        holder_->ope_ = ope;
        return *this;
    }

    Definition& ope(const std::shared_ptr<Ope>& ope) {
        holder_->ope_ = ope;
        return *this;
    }

    template <typename T>
    bool parse(const char* s, size_t l, T& val) const {
        Values v;

        auto m = holder_->parse(s, l, v);
        auto ret = m.ret && m.len == l;

        if (ret && !v.values.empty() && !v.values.front().is_undefined()) {
            val = v.values[0].get<T>();
        }

        return ret;
    }

    template <typename T>
    bool parse(const char* s, T& val) const {
        return parse(s, strlen(s), val);
    }

    bool parse(const char* s, size_t l) const {
        Values v;
        auto m = holder_->parse(s, l, v);
        return m.ret && m.len == l;
    }

    bool parse(const char* s) const {
        return parse(s, strlen(s));
    }

    Definition& operator=(Action ac) {
        assert(!actions.empty());
        actions[0] = ac;
        return *this;
    }

    Definition& operator=(std::initializer_list<Action> acs) {
        actions = acs;
        return *this;
    }

    template <typename T>
    Definition& operator,(T fn) {
        operator=(fn);
        return *this;
    }

    std::string         name;
    std::vector<Action> actions;

private:
    friend class DefinitionReference;

    class Holder : public Ope
    {
    public:
        Holder(Definition* outer)
           : outer_(outer) {}

        Match parse_core(const char* s, size_t l, Values& v) const {
            if (!ope_) {
                throw std::logic_error("Uninitialized definition ope was used...");
            }

            Values chldsv;
            auto m = ope_->parse(s, l, chldsv);
            if (m.ret) {
                v.names.push_back(outer_->name);

                assert(!outer_->actions.empty());

                auto id = m.id + 1;
                const auto& ac = (id < outer_->actions.size() && outer_->actions[id])
                    ? outer_->actions[id]
                    : outer_->actions[0];

                v.values.push_back(reduce(s, m.len, chldsv, ac));
            }
            return m;
        }

    private:
        friend class Definition;

        Any reduce(const char* s, size_t l, const Values& v, const Action& action) const {
            if (action) {
                return action(s, l, v.values, v.names);
            } else if (v.values.empty()) {
                return Any();
            } else {
                return v.values.front();
            }
        }

        std::shared_ptr<Ope> ope_;
        Definition*          outer_;
    };

    Definition& operator=(const Definition& rhs);
    Definition& operator=(Definition&& rhs);

    std::shared_ptr<Holder> holder_;
};

class DefinitionReference : public Ope
{
public:
    DefinitionReference(
        const std::map<std::string, Definition>& grammar, const std::string& name)
        : grammar_(grammar)
        , name_(name) {}

    Match parse_core(const char* s, size_t l, Values& v) const {
        return grammar_.at(name_).holder_->parse(s, l, v);
    }

private:
    const std::map<std::string, Definition>& grammar_;
    const std::string                        name_;
};

typedef Definition rule;

/*
 * Factories
 */
template <typename... Args>
std::shared_ptr<Ope> seq(Args&& ...args) {
    return std::make_shared<Sequence>(static_cast<std::shared_ptr<Ope>>(args)...);
}

template <typename... Args>
std::shared_ptr<Ope> cho(Args&& ...args) {
    return std::make_shared<PrioritizedChoice>(static_cast<std::shared_ptr<Ope>>(args)...);
}

inline std::shared_ptr<Ope> zom(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<ZeroOrMore>(ope);
}

inline std::shared_ptr<Ope> oom(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<OneOrMore>(ope);
}

inline std::shared_ptr<Ope> opt(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Option>(ope);
}

inline std::shared_ptr<Ope> apd(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<AndPredicate>(ope);
}

inline std::shared_ptr<Ope> npd(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<NotPredicate>(ope);
}

inline std::shared_ptr<Ope> lit(const char* lit) {
    return std::make_shared<LiteralString>(lit);
}

inline std::shared_ptr<Ope> cls(const char* chars) {
    return std::make_shared<CharacterClass>(chars);
}

inline std::shared_ptr<Ope> chr(char c) {
    return std::make_shared<Character>(c);
}

inline std::shared_ptr<Ope> any() {
    return std::make_shared<AnyCharacter>();
}

inline std::shared_ptr<Ope> grp(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Grouping>(ope);
}

inline std::shared_ptr<Ope> grp(const std::shared_ptr<Ope>& ope, std::function<void (const char* s, size_t l)> match) {
    return std::make_shared<Grouping>(ope, match);
}

inline std::shared_ptr<Ope> ref(const std::map<std::string, Definition>& grammar, const std::string& name) {
    return std::make_shared<DefinitionReference>(grammar, name);
}

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

typedef std::map<std::string, Definition> Grammar;

inline Grammar make_peg_grammar()
{
    Grammar g;

    // Setup PEG syntax parser
    g["Grammar"]    <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
    g["Definition"] <= seq(g["Identifier"], g["LEFTARROW"], g["Expression"]);

    g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
    g["Sequence"]   <= zom(g["Prefix"]);
    g["Prefix"]     <= seq(opt(cho(g["AND"], g["NOT"])), g["Suffix"]);
    g["Suffix"]     <= seq(g["Primary"], opt(cho(g["QUESTION"], g["STAR"], g["PLUS"])));
    g["Primary"]    <= cho(seq(g["Identifier"], npd(g["LEFTARROW"])),
                           seq(g["OPEN"], g["Expression"], g["CLOSE"]),
                           g["Literal"], g["Class"], g["DOT"]);

    g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
    g["IdentCont"]  <= seq(g["IdentStart"], zom(g["IdentRest"]));
    g["IdentStart"] <= cls("a-zA-Z_");
    g["IdentRest"]  <= cho(g["IdentStart"], cls("0-9"));

    g["Literal"]    <= cho(seq(cls("'"), g["SQCont"], cls("'"), g["Spacing"]),
                           seq(cls("\""), g["DQCont"], cls("\""), g["Spacing"]));
    g["SQCont"]     <= zom(seq(npd(cls("'")), g["Char"]));
    g["DQCont"]     <= zom(seq(npd(cls("\"")), g["Char"]));

    g["Class"]      <= seq(chr('['), g["ClassCont"], chr(']'), g["Spacing"]);
    g["ClassCont"]  <= zom(seq(npd(chr(']')), g["Range"]));

    g["Range"]      <= cho(seq(g["Char"], chr('-'), g["Char"]), g["Char"]);
    g["Char"]       <= cho(seq(chr('\\'), cls("nrt'\"[]\\")),
                           seq(chr('\\'), cls("0-2"), cls("0-7"), cls("0-7")),
                           seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                           seq(npd(chr('\\')), any()));

    g["LEFTARROW"]  <= seq(lit("<-"), g["Spacing"]);
    g["SLASH"]      <= seq(chr('/'), g["Spacing"]);
    g["AND"]        <= seq(chr('&'), g["Spacing"]);
    g["NOT"]        <= seq(chr('!'), g["Spacing"]);
    g["QUESTION"]   <= seq(chr('?'), g["Spacing"]);
    g["STAR"]       <= seq(chr('*'), g["Spacing"]);
    g["PLUS"]       <= seq(chr('+'), g["Spacing"]);
    g["OPEN"]       <= seq(chr('('), g["Spacing"]);
    g["CLOSE"]      <= seq(chr(')'), g["Spacing"]);
    g["DOT"]        <= seq(chr('.'), g["Spacing"]);

    g["Spacing"]    <= zom(cho(g["Space"], g["Comment"]));
    g["Comment"]    <= seq(chr('#'), zom(seq(npd(g["EndOfLine"]), any())), g["EndOfLine"]);
    g["Space"]      <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
    g["EndOfLine"]  <= cho(lit("\r\n"), chr('\n'), chr('\r'));
    g["EndOfFile"]  <= npd(any());

    // Set definition names
    for (auto& x: g) {
        x.second.name = x.first;
    }

    return g;
}

inline std::shared_ptr<Grammar> make_grammar(const char* syntax, std::string& start)
{
    Grammar peg = make_peg_grammar();

    // Prepare output variables
    auto grammar = std::make_shared<Grammar>();
    start.clear();

    // Setup actions
    std::set<std::string> refs;

    peg["Definition"] = [&](const std::vector<Any>& v) {
        const auto& name = v[0].get<std::string>();
        (*grammar)[name] <= v[2].get<std::shared_ptr<Ope>>();
        (*grammar)[name].name = name;

        if (start.empty()) {
            start = name;
        }
    };

    peg["Expression"] = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Ope>> opes;
        for (auto i = 0u; i < v.size(); i++) {
            if (!(i % 2)) {
                opes.push_back(v[i].get<std::shared_ptr<Ope>>());
            }
        }
        return static_cast<std::shared_ptr<Ope>>(std::make_shared<PrioritizedChoice>(opes));
    };

    peg["Sequence"] = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Ope>> opes;
        for (const auto& x: v) {
            opes.push_back(x.get<std::shared_ptr<Ope>>());
        }
        return static_cast<std::shared_ptr<Ope>>(std::make_shared<Sequence>(opes));
    };

    peg["Prefix"] = [&](const std::vector<Any>& v, const std::vector<std::string>& n) {
        std::shared_ptr<Ope> ope;
        if (v.size() == 1) {
            ope = v[0].get<std::shared_ptr<Ope>>();
        } else {
            assert(v.size() == 2);
            ope = v[1].get<std::shared_ptr<Ope>>();
            if (n[0] == "AND") {
                ope = apd(ope);
            } else { // "NOT"
                ope = npd(ope);
            }
        }
        return ope;
    };

    peg["Suffix"] = [&](const std::vector<Any>& v, const std::vector<std::string>& n) {
        auto ope = v[0].get<std::shared_ptr<Ope>>();
        if (v.size() == 1) {
            return ope;
        } else {
            assert(v.size() == 2);
            if (n[1] == "QUESTION") {
                return opt(ope);
            } else if (n[1] == "STAR") {
                return zom(ope);
            } else { // "PLUS"
                return oom(ope);
            }
        }
    };

    peg["Primary"].actions = {
        [&](const std::vector<Any>& v) { return v[0]; },
        [&](const std::vector<Any>& v) { refs.insert(v[0]); return ref(*grammar, v[0]); },
        [&](const std::vector<Any>& v) { return v[1]; }
    };

    peg["IdentCont"] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["Literal"] = [](const std::vector<Any>& v) {
        return lit(v[0].get<std::string>().c_str());
    };
    peg["SQCont"] = [](const char*s, size_t l) {
        return std::string(s, l);
    };
    peg["DQCont"] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["Class"] = [](const std::vector<Any>& v) {
        return cls(v[0].get<std::string>().c_str());
    };
    peg["ClassCont"] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["DOT"] = []() {
        return any();
    };

    if (peg["Grammar"].parse(syntax)) {
        for (const auto& name : refs) {
            if (grammar->find(name) == grammar->end()) {
                return nullptr;
            }
        }
        return grammar;
    }

    return nullptr;
}

/*-----------------------------------------------------------------------------
 *  Parser
 *---------------------------------------------------------------------------*/

class Parser
{
public:
    operator bool() {
        return grammar_ != nullptr;
    }

    bool load_syntax(const char* syntax) {
        grammar_ = make_grammar(syntax, start_);
        return grammar_ != nullptr;
    }

    bool parse(const char* s, Any& val) const {
        if (grammar_ != nullptr)
            return (*grammar_)[start_].parse(s, val);
        return false;
    }

    bool parse(const char* s) const {
        if (grammar_ != nullptr) {
            Any val;
            return (*grammar_)[start_].parse(s, val);
        }
        return false;
    }

    template <typename T>
    bool parse(const char* s, T& out) const {
        if (grammar_ != nullptr) {
            Any val;
            auto ret = (*grammar_)[start_].parse(s, val);
            if (ret) {
                out = val.get<T>();
            }
            return ret;
        }
        return false;
    }

    Definition& operator[](const char* s) {
        return (*grammar_)[s];
    }

private:
    std::shared_ptr<Grammar> grammar_;
    std::string              start_;
};

inline Parser make_parser(const char* syntax) {
    Parser parser;
    if (!parser.load_syntax(syntax)) {
        throw std::logic_error("PEG syntax error.");
    }
    return parser;
}

} // namespace peglib

#endif

// vim: et ts=4 sw=4 cin cino={1s ff=unix
