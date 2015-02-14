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
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>

namespace peglib {

void* enabler;

/*-----------------------------------------------------------------------------
 *  any
 *---------------------------------------------------------------------------*/

class any
{
public:
    any() : content_(nullptr) {}

    any(const any& rhs) : content_(rhs.clone()) {}

    any(any&& rhs) : content_(rhs.content_) {
        rhs.content_ = nullptr;
    }

    template <typename T>
    any(const T& value) : content_(new holder<T>(value)) {}

    any& operator=(const any& rhs) {
        if (this != &rhs) {
            if (content_) {
                delete content_;
            }
            content_ = rhs.clone();
        }
        return *this;
    }

    any& operator=(any&& rhs) {
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
    any& operator=(const T& value) {
        if (content_) {
            delete content_;
        }
        content_ = new holder<T>(value);
        return *this;
    }

    ~any() {
        delete content_;
    }

    bool is_undefined() const {
        return content_ == nullptr;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, any>::value>::type*& = enabler
    >
    T& get() {
        assert(content_);
        auto p = dynamic_cast<holder<T>*>(content_);
        assert(p);
        if (!p) {
            throw std::bad_cast();
        }
        return p->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, any>::value>::type*& = enabler
    >
    T& get() {
        return *this;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, any>::value>::type*& = enabler
    >
    const T& get() const {
        assert(content_);
        auto p = dynamic_cast<holder<T>*>(content_);
        assert(p);
        if (!p) {
            throw std::bad_cast();
        }
        return p->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, any>::value>::type*& = enabler
    >
    const any& get() const {
        return *this;
    }

    operator bool() const { return get<bool>(); }
    operator char() const { return get<char>(); }
    operator wchar_t() const { return get<wchar_t>(); }
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

#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
#else
    operator char16_t() const { return get<char16_t>(); }
    operator char32_t() const { return get<char32_t>(); }
#endif

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
    std::vector<any>         values;
};

/*
 * Semantic action
 */
template <
    typename R, typename F,
    typename std::enable_if<!std::is_void<R>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return any(fn(std::forward<Args>(args)...));
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_void<R>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    fn(std::forward<Args>(args)...);
    return any();
}

class Action
{
public:
    Action() = default;

    Action(const Action& rhs) : fn_(rhs.fn_) {}

    //Action(Action&& rhs) : fn_(std::move(rhs.fn_)) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, &F::operator())) {}

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, fn)) {}

    template <typename F, typename std::enable_if<std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    Action(F fn) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, &F::operator());
    }

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, fn);
    }

    template <typename F, typename std::enable_if<std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    void operator=(F fn) {}

    operator bool() const {
        return (bool)fn_;
    }

    any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) const {
        return fn_(s, l, v, n);
    }

private:
    template <typename R>
    struct TypeAdaptor {
        TypeAdaptor(std::function<R (const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n)> fn)
            : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l, v, n);
        }
        std::function<R (const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l_v {
        TypeAdaptor_s_l_v(std::function<R (const char* s, size_t l, const std::vector<any>& v)> fn)
            : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l, v);
        }
        std::function<R (const char* s, size_t l, const std::vector<any>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l {
        TypeAdaptor_s_l(std::function<R (const char* s, size_t l)> fn) : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, s, l);
        }
        std::function<R (const char* s, size_t l)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v_n {
        TypeAdaptor_v_n(std::function<R (const std::vector<any>& v, const std::vector<std::string>& n)> fn) : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, v, n);
        }
        std::function<R (const std::vector<any>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v {
        TypeAdaptor_v(std::function<R (const std::vector<any>& v)> fn) : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_, v);
        }
        std::function<R (const std::vector<any>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_empty {
        TypeAdaptor_empty(std::function<R ()> fn) : fn_(fn) {}
        any operator()(const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n) {
            return call<R>(fn_);
        }
        std::function<R ()> fn_;
    };

    typedef std::function<any (const char* s, size_t l, const std::vector<any>& v, const std::vector<std::string>& n)> Fty;

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<any>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<any>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<any>& v) const) {
        return TypeAdaptor_s_l_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<any>& v)) {
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
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<any>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<any>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<any>& v) const) {
        return TypeAdaptor_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<any>& v)) {
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
 * Result
 */
struct Result
{
    bool              ret;
    size_t            len;
    size_t            choice;
    const char*       ptr;
    const std::string err; // TODO: should be `int`.
};

Result success(size_t len, size_t choice = 0) {
    return Result{ true, len, choice, nullptr, std::string() };
}

Result fail(const char* ptr, std::string err = std::string(), std::string name = std::string()) {
    return Result{ false, 0, (size_t)-1, ptr, err };
}

/*
 * Parser operators
 */
class Ope
{
public:
    virtual ~Ope() {};
    virtual Result parse(const char* s, size_t l, Values& v) const = 0;
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

    Result parse(const char* s, size_t l, Values& v) const {
        size_t i = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            auto r = rule.parse(s + i, l - i, v);
            if (!r.ret) {
                auto err = r.err;
                if (err.empty()) {
                    err = "missing an element in the 'sequence'";
                }
                return fail(r.ptr, err);
            }
            i += r.len;
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

    Result parse(const char* s, size_t l, Values& v) const {
        size_t id = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            Values chldsv;
            auto r = rule.parse(s, l, chldsv);
            if (r.ret) {
                if (!chldsv.values.empty()) {
                    for (const auto& x: chldsv.values) {
                        v.values.push_back(x);
                    }
                    for (const auto& x: chldsv.names) {
                        v.names.push_back(x);
                    }
                }
                return success(r.len, id);
            }
            id++;
        }
        return fail(s, "nothing was matched in the 'prioritized choice'");
    }

    size_t size() const { return opes_.size();  }

private:
    std::vector<std::shared_ptr<Ope>> opes_;
};

class ZeroOrMore : public Ope
{
public:
    ZeroOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Result parse(const char* s, size_t l, Values& v) const {
        auto i = 0;
        while (l - i > 0) {
            const auto& rule = *ope_;
            auto r = rule.parse(s + i, l - i, v);
            if (!r.ret) {
                break;
            }
            i += r.len;
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

    Result parse(const char* s, size_t l, Values& v) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, v);
        if (!r.ret) {
            auto err = r.err;
            if (err.empty()) {
                err = "nothing occurred in the 'one-or-more'";
            }
            return fail(r.ptr, r.err);
        }
        auto i = r.len;
        while (l - i > 0) {
            const auto& rule = *ope_;
            auto r = rule.parse(s + i, l - i, v);
            if (!r.ret) {
                break;
            }
            i += r.len;
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

    Result parse(const char* s, size_t l, Values& v) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, v);
        return success(r.ret ? r.len : 0);
    }

private:
    std::shared_ptr<Ope> ope_;
};

class AndPredicate : public Ope
{
public:
    AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Result parse(const char* s, size_t l, Values& v) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, v);
        if (r.ret) {
            return success(0);
        } else {
            return fail(r.ptr, r.err);
        }
    }

private:
    std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope
{
public:
    NotPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Result parse(const char* s, size_t l, Values& v) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, v);
        if (r.ret) {
            return fail(s);
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
    LiteralString(const std::string& s) : lit_(s) {}

    Result parse(const char* s, size_t l, Values& v) const {
        auto i = 0u;
        for (; i < lit_.size(); i++) {
            if (i >= l || s[i] != lit_[i]) {
                return fail(s);
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
    CharacterClass(const std::string& chars) : chars_(chars) {}

    Result parse(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1) {
            return fail(s);
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
        return fail(s);
    }

private:
    std::string chars_;
};

class Character : public Ope
{
public:
    Character(char ch) : ch_(ch) {}

    Result parse(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1 || s[0] != ch_) {
            return fail(s);
        }
        return success(1);
    }

private:
    char ch_;
};

class AnyCharacter : public Ope
{
public:
    Result parse(const char* s, size_t l, Values& v) const {
        // TODO: UTF8 support
        if (l < 1) {
            return fail(s);
        }
        return success(1);
    }

};

class Grouping : public Ope
{
public:
    Grouping(const std::shared_ptr<Ope>& ope) : ope_(ope) {}
    Grouping(const std::shared_ptr<Ope>& ope, std::function<void(const char* s, size_t l)> match) : ope_(ope), match_(match) {}

    Result parse(const char* s, size_t l, Values& v) const {
        assert(ope_);
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, v);
        if (r.ret && match_) {
            match_(s, r.len);
        }
        return r;
    }

private:
    std::shared_ptr<Ope>                        ope_;
    std::function<void(const char* s, size_t l)> match_;
};

class WeakHolder : public Ope
{
public:
    WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

    Result parse(const char* s, size_t l, Values& v) const {
        auto ope = weak_.lock();
        assert(ope);
        const auto& rule = *ope;
        return rule.parse(s, l, v);
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

    template <typename T>
    Result parse(const char* s, size_t l, T& val) const {
        Values v;
        auto r = holder_->parse(s, l, v);
        if (r.ret && !v.values.empty() && !v.values.front().is_undefined()) {
            val = v.values[0].get<T>();
        }
        return r;
    }

    template <typename T>
    Result parse(const char* s, T& val) const {
        auto l = strlen(s);
        return parse(s, l, val);
    }

    Result parse(const char* s) const {
        auto l = strlen(s);
        Values v;
        return holder_->parse(s, l, v);
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

        Result parse(const char* s, size_t l, Values& v) const {
            if (!ope_) {
                throw std::logic_error("Uninitialized definition ope was used...");
            }

            const auto& rule = *ope_;
            Values chldsv;
            auto r = rule.parse(s, l, chldsv);
            if (r.ret) {
                v.names.push_back(outer_->name);

                assert(!outer_->actions.empty());

                auto id = r.choice + 1;
                const auto& ac = (id < outer_->actions.size() && outer_->actions[id])
                    ? outer_->actions[id]
                    : outer_->actions[0];

                v.values.push_back(reduce(s, r.len, chldsv, ac));
            }
            return r;
        }

    private:
        friend class Definition;

        any reduce(const char* s, size_t l, const Values& v, const Action& action) const {
            if (action) {
                return action(s, l, v.values, v.names);
            } else if (v.values.empty()) {
                return any();
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

    Result parse(const char* s, size_t l, Values& v) const {
        const auto& rule = *grammar_.at(name_).holder_;
        return rule.parse(s, l, v);
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

inline std::shared_ptr<Ope> lit(const std::string& lit) {
    return std::make_shared<LiteralString>(lit);
}

inline std::shared_ptr<Ope> cls(const std::string& chars) {
    return std::make_shared<CharacterClass>(chars);
}

inline std::shared_ptr<Ope> chr(char c) {
    return std::make_shared<Character>(c);
}

inline std::shared_ptr<Ope> dot() {
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

inline std::pair<size_t, size_t> line_info(const char* s, const char* ptr) {
    auto p = s;
    auto col_ptr = p;
    auto no = 1;

    while (p < ptr) {
        if (*p == '\n') {
            no++;
            col_ptr = p + 1;
        }
        p++;
    }

    auto col = p - col_ptr + 1;

    return std::make_pair(no, col);
}

typedef std::map<std::string, Definition> Grammar;
typedef std::function<void (size_t, size_t, const std::string&)> Log;

class PEGParser
{
public:
    static std::shared_ptr<Grammar> parse(const char* s, size_t l, std::string& start, Log log) {
        static PEGParser instance;
        return get().perform_core(s, l, start, log);
    }

    static Grammar& grammar() {
        return get().g;
    }

private:
    static PEGParser& get() {
        static PEGParser instance;
        return instance;
    }

    PEGParser() {
        make_grammar();
        setup_actions();
    }

    void make_grammar() {
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
                               seq(chr('\\'), cls("0-2"), cls("0-7"), cls("0-7")), // TODO: 0-2 should be 0-3. bug in the spec...
                               seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                               seq(npd(chr('\\')), dot()));

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
        g["Comment"]    <= seq(chr('#'), zom(seq(npd(g["EndOfLine"]), dot())), g["EndOfLine"]);
        g["Space"]      <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
        g["EndOfLine"]  <= cho(lit("\r\n"), chr('\n'), chr('\r'));
        g["EndOfFile"]  <= npd(dot());

        // Set definition names
        for (auto& x: g) {
            x.second.name = x.first;
        }
    }

    void setup_actions() {

        g["Expression"] = [&](const std::vector<any>& v) {
            if (v.size() == 1) {
                return v[0].get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (auto i = 0u; i < v.size(); i++) {
                    if (!(i % 2)) {
                        opes.push_back(v[i].get<std::shared_ptr<Ope>>());
                    }
                }
                const std::shared_ptr<Ope> ope = std::make_shared<PrioritizedChoice>(opes);
                return ope;
            }
        };

        g["Sequence"] = [&](const std::vector<any>& v) {
            if (v.size() == 1) {
                return v[0].get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (const auto& x: v) {
                    opes.push_back(x.get<std::shared_ptr<Ope>>());
                }
                const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
                return ope;
            }
        };

        g["Prefix"] = [&](const std::vector<any>& v, const std::vector<std::string>& n) {
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

        g["Suffix"] = [&](const std::vector<any>& v, const std::vector<std::string>& n) {
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

        g["IdentCont"] = [](const char*s, size_t l) {
            return std::string(s, l);
        };

        g["Literal"] = [](const std::vector<any>& v) {
            return lit(v[0]);
        };
        g["SQCont"] = [this](const char*s, size_t l) {
            return resolve_escape_sequence(s, l);
        };
        g["DQCont"] = [this](const char*s, size_t l) {
            return resolve_escape_sequence(s, l);
        };

        g["Class"] = [](const std::vector<any>& v) {
            return cls(v[0]);
        };
        g["ClassCont"] = [this](const char*s, size_t l) {
            return resolve_escape_sequence(s, l);
        };

        g["DOT"] = []() {
            return dot();
        };
    }

    std::shared_ptr<Grammar> perform_core(const char* s, size_t l, std::string& start, Log log) {
        auto grammar = std::make_shared<Grammar>();
        start.clear();
        std::map<std::string, const char*> refs;

        g["Definition"] = [&](const std::vector<any>& v) {
            const auto& name = v[0].get<std::string>();
            (*grammar)[name] <= v[2].get<std::shared_ptr<Ope>>();
            (*grammar)[name].name = name;

            if (start.empty()) {
                start = name;
            }
        };

        g["Primary"].actions = {
            [&](const std::vector<any>& v) {
                return v[0];
            },
            [&](const char* s, size_t l, const std::vector<any>& v) {
                refs[v[0]] = s;
                return ref(*grammar, v[0]);
            },
            [&](const std::vector<any>& v) {
                return v[1];
            }
        };

        auto r = g["Grammar"].parse(s, l);
        if (!r.ret) {
            if (log) {
                auto line = line_info(s, r.ptr);
                log(line.first, line.second, r.err.empty() ? "syntax error" : r.err);
            }
            return nullptr;
        }

        for (const auto& x : refs) {
            const auto& name = x.first;
            auto ptr = x.second;
            if (grammar->find(name) == grammar->end()) {
                if (log) {
                    auto line = line_info(s, ptr);
                    log(line.first, line.second, "'" + name + "' is not defined.");
                }
                return nullptr;
            }
        }

        return grammar;
    }

    std::string resolve_escape_sequence(const char*s, size_t l) {
        std::string r;
        r.reserve(l);

        for (auto i = 0u; i < l; i++) {
            auto ch = s[i];
            if (ch == '\\') {
                i++; 
                switch (s[i]) {
                    case 'n':  r += '\n'; break;
                    case 'r':  r += '\r'; break;
                    case 't':  r += '\t'; break;
                    case '\'': r += '\''; break;
                    case '"':  r += '"';  break;
                    case '[':  r += '[';  break;
                    case ']':  r += ']';  break;
                    case '\\': r += '\\'; break;
                    default: {
                        // TODO: Octal number support
                        assert(false);
                        break;
                    }
                }
            } else {
                r += ch;
            }
        }
        return r;
    }

    Grammar g;
};

/*-----------------------------------------------------------------------------
 *  peg
 *---------------------------------------------------------------------------*/

class peg
{
public:
    peg(const char* s, size_t l, Log log = nullptr) {
        grammar_ = PEGParser::parse(s, l, start_, log);
    }

    peg(const char* s, Log log = nullptr) {
        auto l = strlen(s);
        grammar_ = PEGParser::parse(s, l, start_, log);
    }

    operator bool() {
        return grammar_ != nullptr;
    }

    template <typename T>
    bool match(const char* s, size_t l, T& out, bool exact = true) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l, out);
            return r.ret && (!exact || r.len == l);
        }
        return false;
    }

    bool match(const char* s, size_t l, bool exact = true) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l);
            return r.ret && (!exact || r.len == l);
        }
        return false;
    }

    template <typename T>
    bool match(const char* s, T& out, bool exact = true) const {
        auto l = strlen(s);
        return match(s, l, out, exact);
    }

    bool match(const char* s, bool exact = true) const {
        auto l = strlen(s);
        return match(s, l, exact);
    }

    bool lint(const char* s, size_t l, bool exact, Log log = nullptr) {
        assert(grammar_);
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l);
            if (!r.ret) {
                if (log) {
                    auto line = line_info(s, r.ptr);
                    log(line.first, line.second, r.err.empty() ? "syntax error" : r.err);
                }
            } else if (exact && r.len != l) {
                auto line = line_info(s, s + r.len);
                log(line.first, line.second, "garbage string at the end");
            }
            return r.ret && (!exact || r.len == l);
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

} // namespace peglib

#endif

// vim: et ts=4 sw=4 cin cino={1s ff=unix
