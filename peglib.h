//
//  peglib.h
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef _CPPPEGLIB_PEGLIB_H_
#define _CPPPEGLIB_PEGLIB_H_

#include <functional>
#include <string>
#include <limits>
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
struct SemanticValue {
    any         val;
    std::string name;
    const char* s;
    size_t      l;
};

struct SemanticValues : protected std::vector<SemanticValue>
{
    const char* s;
    size_t      l;

    SemanticValues() : s(nullptr), l(0) {}

    typedef SemanticValue T;
    using std::vector<T>::iterator;
    using std::vector<T>::const_iterator;
    using std::vector<T>::size;
    using std::vector<T>::empty;
    using std::vector<T>::assign;
    using std::vector<T>::begin;
    using std::vector<T>::end;
    using std::vector<T>::rbegin;
    using std::vector<T>::rend;
    using std::vector<T>::operator[];
    using std::vector<T>::at;
    using std::vector<T>::resize;
    using std::vector<T>::front;
    using std::vector<T>::back;
    using std::vector<T>::push_back;
    using std::vector<T>::pop_back;
    using std::vector<T>::insert;
    using std::vector<T>::erase;
    using std::vector<T>::clear;
    using std::vector<T>::swap;

    template <typename T, typename U, typename V>
    static U reduce(T i, T end, U val, V f){
        if (i == end) {
            return val;
        }
        std::tie(val, i) = f(val, i);
        return reduce(i, end, val, f);
    };
};

/*
 * Semantic action
 */
template <
    typename R, typename F,
    typename std::enable_if<std::is_void<R>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    fn(std::forward<Args>(args)...);
    return any();
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_same<R, any>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return fn(std::forward<Args>(args)...);
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_same<R, SemanticValue>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return fn(std::forward<Args>(args)...).val;
}

template <
    typename R, typename F,
    typename std::enable_if<!std::is_void<R>::value && !std::is_same<R, any>::value && !std::is_same<R, SemanticValue>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return any(fn(std::forward<Args>(args)...));
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

    any operator()(const SemanticValues& sv, any& dt) const {
        return fn_(sv, dt);
    }

private:
    template <typename R>
    struct TypeAdaptor {
        TypeAdaptor(std::function<R (const SemanticValues& sv)> fn)
            : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& dt) {
            return call<R>(fn_, sv);
        }
        std::function<R (const SemanticValues& sv)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_c {
        TypeAdaptor_c(std::function<R (const SemanticValues& sv, any& dt)> fn)
            : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& dt) {
            return call<R>(fn_, sv, dt);
        }
        std::function<R (const SemanticValues& sv, any& dt)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l {
        TypeAdaptor_s_l(std::function<R (const char* s, size_t l)> fn) : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& dt) {
            return call<R>(fn_, sv.s, sv.l);
        }
        std::function<R (const char* s, size_t l)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_empty {
        TypeAdaptor_empty(std::function<R ()> fn) : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& dt) {
            return call<R>(fn_);
        }
        std::function<R ()> fn_;
    };

    typedef std::function<any (const SemanticValues& sv, any& dt)> Fty;

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const SemanticValues& sv) const) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const SemanticValues& sv)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const SemanticValues& sv)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const SemanticValues& sv, any& dt) const) {
        return TypeAdaptor_c<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const SemanticValues& sv, any& dt)) {
        return TypeAdaptor_c<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const SemanticValues& sv, any& dt)) {
        return TypeAdaptor_c<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t) const) {
        return TypeAdaptor_s_l<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t)) {
        return TypeAdaptor_s_l<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const char*, size_t)) {
        return TypeAdaptor_s_l<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)() const) {
        return TypeAdaptor_empty<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)()) {
        return TypeAdaptor_empty<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)()) {
        return TypeAdaptor_empty<R>(fn);
    }

    Fty fn_;
};

/*
 * Match action
 */
typedef std::function<void (const char* s, size_t l, size_t i)> MatchAction;

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
    virtual Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const = 0;
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        size_t i = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            auto r = rule.parse(s + i, l - i, sv, dt);
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        size_t id = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            SemanticValues chldsv;
            auto r = rule.parse(s, l, chldsv, dt);
            if (r.ret) {
                if (!chldsv.empty()) {
                    sv.insert(sv.end(), chldsv.begin(), chldsv.end());
                }
                sv.s = chldsv.s;
                sv.l = chldsv.l;
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        auto i = 0;
        while (l - i > 0) {
            const auto& rule = *ope_;
            auto r = rule.parse(s + i, l - i, sv, dt);
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
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
            auto r = rule.parse(s + i, l - i, sv, dt);
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
        return success(r.ret ? r.len : 0);
    }

private:
    std::shared_ptr<Ope> ope_;
};

class AndPredicate : public Ope
{
public:
    AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
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
    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        // TODO: UTF8 support
        if (l < 1) {
            return fail(s);
        }
        return success(1);
    }

};

class Capture : public Ope
{
public:
    Capture(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t ci)
        : ope_(ope), match_action_(ma), capture_id(ci) {}

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        assert(ope_);
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
        if (r.ret && match_action_) {
            match_action_(s, r.len, capture_id);
        }
        return r;
    }

private:
    std::shared_ptr<Ope> ope_;
    MatchAction          match_action_;
    size_t               capture_id;
};

class Anchor : public Ope
{
public:
    Anchor(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        assert(ope_);
        const auto& rule = *ope_;
        auto r = rule.parse(s, l, sv, dt);
        if (r.ret) {
            sv.s = s;
            sv.l = r.len;
        }
        return r;
    }

private:
    std::shared_ptr<Ope> ope_;
};

typedef std::function<Result (const char* s, size_t l, SemanticValues& sv, any& dt)> Parser;

class User : public Ope
{
public:
    User(Parser fn) : fn_(fn) {}

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        assert(fn_);
        return fn_(s, l, sv, dt);
    }

private:
    std::function<Result (const char* s, size_t l, SemanticValues& sv, any& dt)> fn_;
};

class WeakHolder : public Ope
{
public:
    WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        auto ope = weak_.lock();
        assert(ope);
        const auto& rule = *ope;
        return rule.parse(s, l, sv, dt);
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
       , ignore(false)
       , holder_(std::make_shared<Holder>(this)) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , actions(1)
        , ignore(false)
        , holder_(rhs.holder_)
    {
        holder_->outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , actions(1)
        , ignore(rhs.ignore)
        , holder_(std::move(rhs.holder_))
    {
        holder_->outer_ = this;
    }

    Definition(const std::shared_ptr<Ope>& ope)
        : actions(1)
        , ignore(false)
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        return holder_->parse(s, l, sv, dt);
    }

    template <typename T>
    Result parse(const char* s, size_t l, T& val) const {
        SemanticValues sv;
        any dt;
        auto r = holder_->parse(s, l, sv, dt);
        if (r.ret && !sv.empty() && !sv.front().val.is_undefined()) {
            val = sv[0].val.get<T>();
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
        SemanticValues sv;
        any dt;
        return holder_->parse(s, l, sv, dt);
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

    Definition& operator~() {
        ignore = true;
        return *this;
    }

    std::string         name;
    std::vector<Action> actions;
    bool                ignore;

private:
    friend class DefinitionReference;

    class Holder : public Ope
    {
    public:
        Holder(Definition* outer)
           : outer_(outer) {}

        Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
            if (!ope_) {
                throw std::logic_error("Uninitialized definition ope was used...");
            }

            const auto& rule = *ope_;
            SemanticValues chldsv;
            auto r = rule.parse(s, l, chldsv, dt);
            if (r.ret && !outer_->ignore) {
                assert(!outer_->actions.empty());

                auto id = r.choice + 1;
                const auto& action = (id < outer_->actions.size() && outer_->actions[id])
                    ? outer_->actions[id]
                    : outer_->actions[0];

                if (!chldsv.s) {
                    chldsv.s = s;
                    chldsv.l = r.len;
                }
                auto val = reduce(chldsv, dt, action);

                sv.push_back(SemanticValue{ val, outer_->name, nullptr, 0 });
            }
            return r;
        }

    private:
        friend class Definition;

        any reduce(const SemanticValues& sv, any& dt, const Action& action) const {
            if (action) {
                return action(sv, dt);
            } else if (sv.empty()) {
                return any();
            } else {
                return sv.front().val;
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

    Result parse(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        const auto& rule = *grammar_.at(name_).holder_;
        return rule.parse(s, l, sv, dt);
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

inline std::shared_ptr<Ope> chr(char dt) {
    return std::make_shared<Character>(dt);
}

inline std::shared_ptr<Ope> dot() {
    return std::make_shared<AnyCharacter>();
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t ci) {
    return std::make_shared<Capture>(ope, ma, ci);
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma) {
    return std::make_shared<Capture>(ope, ma, (size_t)-1);
}

inline std::shared_ptr<Ope> anc(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Anchor>(ope);
}

inline std::shared_ptr<Ope> usr(std::function<Result (const char* s, size_t l, SemanticValues& sv, any& dt)> fn) {
    return std::make_shared<User>(fn);
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

typedef std::map<std::string, std::shared_ptr<Ope>> Rules;

class PEGParser
{
public:
    static std::shared_ptr<Grammar> parse(
        const char*  s,
        size_t       l,
        const Rules& rules,
        std::string& start,
        MatchAction  ma,
        Log          log)
    {
        static PEGParser instance;
        return get().perform_core(s, l, rules, start, ma, log);
    }

    static std::shared_ptr<Grammar> parse(
        const char*  s,
        size_t       l,
        std::string& start,
        MatchAction  ma,
        Log          log)
    {
        Rules dummy;
        return parse(s, l, dummy, start, ma, log);
    }

    // For debuging purpose
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

    struct Context {
        std::shared_ptr<Grammar>           grammar;
        std::string                        start;
        MatchAction                        match_action;
        std::map<std::string, const char*> references;
        size_t                             capture_count;

        Context() : grammar(std::make_shared<Grammar>()), capture_count(0) {}
    };

    void make_grammar() {
        // Setup PEG syntax parser
        g["Grammar"]    <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
        g["Definition"] <= seq(opt(g["IGNORE"]), g["Identifier"], g["LEFTARROW"], g["Expression"]);

        g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
        g["Sequence"]   <= zom(g["Prefix"]);
        g["Prefix"]     <= seq(opt(cho(g["AND"], g["NOT"])), g["Suffix"]);
        g["Suffix"]     <= seq(g["Primary"], opt(cho(g["QUESTION"], g["STAR"], g["PLUS"])));
        g["Primary"]    <= cho(seq(g["Identifier"], npd(g["LEFTARROW"])),
                               seq(g["OPEN"], g["Expression"], g["CLOSE"]),
                               seq(g["Begin"], g["Expression"], g["End"]),
                               seq(g["BeginCap"], g["Expression"], g["EndCap"]),
                               g["Literal"], g["Class"], g["DOT"]);

        g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
        g["IdentCont"]  <= seq(g["IdentStart"], zom(g["IdentRest"]));
        g["IdentStart"] <= cls("a-zA-Z_");
        g["IdentRest"]  <= cho(g["IdentStart"], cls("0-9"));

        g["Literal"]    <= cho(seq(cls("'"), anc(zom(seq(npd(cls("'")), g["Char"]))), cls("'"), g["Spacing"]),
                               seq(cls("\""), anc(zom(seq(npd(cls("\"")), g["Char"]))), cls("\""), g["Spacing"]));

        g["Class"]      <= seq(chr('['), anc(zom(seq(npd(chr(']')), g["Range"]))), chr(']'), g["Spacing"]);

        g["Range"]      <= cho(seq(g["Char"], chr('-'), g["Char"]), g["Char"]);
        g["Char"]       <= cho(seq(chr('\\'), cls("nrt'\"[]\\")),
                               seq(chr('\\'), cls("0-2"), cls("0-7"), cls("0-7")), // TODO: 0-2 should be 0-3. bug in the spec...
                               seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                               seq(npd(chr('\\')), dot()));

        g["LEFTARROW"]  <= seq(lit("<-"), g["Spacing"]);
        ~g["SLASH"]     <= seq(chr('/'), g["Spacing"]);
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

        g["Begin"]      <= seq(chr('<'), g["Spacing"]);
        g["End"]        <= seq(chr('>'), g["Spacing"]);

        g["BeginCap"]   <= seq(lit("$<"), g["Spacing"]);
        g["EndCap"]     <= seq(lit(">"), g["Spacing"]);

        g["IGNORE"]     <= chr('~');

        g["Action"]     <= seq(chr('{'), anc(zom(npd(chr('}')))), chr('}'), g["Spacing"]);

        // Set definition names
        for (auto& x: g) {
            x.second.name = x.first;
        }
    }

    void setup_actions() {
        g["Definition"] = [&](const SemanticValues& sv, any& dt) {
            Context& cxt = *dt.get<Context*>();

            auto ignore = (sv.size() == 4);
            auto baseId = ignore ? 1 : 0;

            const auto& name = sv[baseId].val.get<std::string>();
            auto ope = sv[baseId + 2].val.get<std::shared_ptr<Ope>>();

            auto& def = (*cxt.grammar)[name];
            def <= ope;
            def.name = name;
            def.ignore = ignore;

            if (cxt.start.empty()) {
                cxt.start = name;
            }
        };

        g["Expression"] = [&](const SemanticValues& sv) {
            if (sv.size() == 1) {
                return sv[0].val.get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (auto i = 0u; i < sv.size(); i++) {
                    opes.push_back(sv[i].val.get<std::shared_ptr<Ope>>());
                }
                const std::shared_ptr<Ope> ope = std::make_shared<PrioritizedChoice>(opes);
                return ope;
            }
        };

        g["Sequence"] = [&](const SemanticValues& sv) {
            if (sv.size() == 1) {
                return sv[0].val.get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (const auto& x: sv) {
                    opes.push_back(x.val.get<std::shared_ptr<Ope>>());
                }
                const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
                return ope;
            }
        };

        g["Prefix"] = [&](const SemanticValues& sv, any& dt) {
            std::shared_ptr<Ope> ope;
            if (sv.size() == 1) {
                ope = sv[0].val.get<std::shared_ptr<Ope>>();
            } else {
                assert(sv.size() == 2);
                auto tok = sv[0].val.get<char>();
                ope = sv[1].val.get<std::shared_ptr<Ope>>();
                if (tok == '&') {
                    ope = apd(ope);
                } else { // '!'
                    ope = npd(ope);
                }
            }
            return ope;
        };

        g["Suffix"] = [&](const SemanticValues& sv, any& dt) {
            auto ope = sv[0].val.get<std::shared_ptr<Ope>>();
            if (sv.size() == 1) {
                return ope;
            } else {
                assert(sv.size() == 2);
                auto tok = sv[1].val.get<char>();
                if (tok == '?') {
                    return opt(ope);
                } else if (tok == '*') {
                    return zom(ope);
                } else { // '+'
                    return oom(ope);
                }
            }
        };

        g["Primary"].actions = {
            [&](const SemanticValues& sv) {
                return sv[0];
            },
            [&](const SemanticValues& sv, any& dt) {
                Context& cxt = *dt.get<Context*>();
                const auto& ident = sv[0].val.get<std::string>();
                cxt.references[ident] = sv.s; // for error handling
                return ref(*cxt.grammar, ident);
            },
            [&](const SemanticValues& sv) {
                return sv[1];
            },
            // Anchor
            [&](const SemanticValues& sv) {
                auto ope = sv[1].val.get<std::shared_ptr<Ope>>();
                return anc(ope);
            },
            // Capture
            [&](const SemanticValues& sv, any& dt) {
                Context& cxt = *dt.get<Context*>();
                auto ope = sv[1].val.get<std::shared_ptr<Ope>>();
                return cap(ope, cxt.match_action, ++cxt.capture_count);
            }
        };

        g["IdentCont"] = [](const char* s, size_t l) {
            return std::string(s, l);
        };
        g["Literal"] = [this](const char* s, size_t l) {
            return lit(resolve_escape_sequence(s, l));
        };
        g["Class"] = [this](const char* s, size_t l) {
            return cls(resolve_escape_sequence(s, l));
        };

        g["AND"]      = [](const char* s, size_t l) { return *s; };
        g["NOT"]      = [](const char* s, size_t l) { return *s; };
        g["QUESTION"] = [](const char* s, size_t l) { return *s; };
        g["STAR"]     = [](const char* s, size_t l) { return *s; };
        g["PLUS"]     = [](const char* s, size_t l) { return *s; };

        g["DOT"] = []() { return dot(); };
    }

    std::shared_ptr<Grammar> perform_core(
        const char*  s,
        size_t       l,
        const Rules& rules,
        std::string& start,
        MatchAction  ma,
        Log          log)
    {
        Context cxt;
        cxt.match_action = ma;

        SemanticValues sv;
        any dt = &cxt;
        auto r = g["Grammar"].parse(s, l, sv, dt);

        if (!r.ret) {
            if (log) {
                auto line = line_info(s, r.ptr);
                log(line.first, line.second, r.err.empty() ? "syntax error" : r.err);
            }
            return nullptr;
        }

        auto& grammar = *cxt.grammar;

        // User provided rules
        for (const auto& x: rules) {
            auto name = x.first;

            bool ignore = false;
            if (!name.empty() && name[0] == '~') {
                ignore = true;
                name.erase(0, 1);
            }

            if (!name.empty()) {
                auto& def = grammar[name];
                def <= x.second;
                def.name = name;
                def.ignore = ignore;
            }
        }

        // Check missing definitions
        for (const auto& x : cxt.references) {
            const auto& name = x.first;
            auto ptr = x.second;
            if (grammar.find(name) == grammar.end()) {
                if (log) {
                    auto line = line_info(s, ptr);
                    log(line.first, line.second, "'" + name + "' is not defined.");
                }
                return nullptr;
            }
        }

        start = cxt.start;

        return cxt.grammar;
    }

    std::string resolve_escape_sequence(const char* s, size_t l) {
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
    peg() = default;

    peg(const char* s, size_t l, const Rules& rules, Log log = nullptr) {
        grammar_ = PEGParser::parse(
            s, l, rules,
            start_,
            [&](const char* s, size_t l, size_t i) {
                if (match_action) match_action(s, l, i);
            },
            log);
    }

    peg(const char* s, const Rules& rules, Log log = nullptr)
        : peg(s, strlen(s), rules, log) {}

    peg(const char* s, size_t l, Log log = nullptr)
        : peg(s, l, Rules(), log) {}

    peg(const char* s, Log log = nullptr)
        : peg(s, strlen(s), Rules(), log) {}

    operator bool() {
        return grammar_ != nullptr;
    }

    template <typename T>
    bool parse(const char* s, size_t l, T& out, bool exact = true) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l, out);
            return r.ret && (!exact || r.len == l);
        }
        return false;
    }

    template <typename T>
    bool parse(const char* s, T& out, bool exact = true) const {
        auto l = strlen(s);
        return parse(s, l, out, exact);
    }

    bool parse(const char* s, size_t l, bool exact = true) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l);
            return r.ret && (!exact || r.len == l);
        }
        return false;
    }

    bool parse(const char* s, bool exact = true) const {
        auto l = strlen(s);
        return parse(s, l, exact);
    }

    bool search(const char* s, size_t l, size_t& mpos, size_t& mlen) const {
        const auto& rule = (*grammar_)[start_];
        if (grammar_ != nullptr) {
            size_t pos = 0;
            while (pos < l) {
                size_t len = l - pos;
                auto r = rule.parse(s + pos, len);
                if (r.ret) {
                    mpos = pos;
                    mlen = r.len;
                    return true;
                }
                pos++;
            }
        }
        mpos = 0;
        mlen = 0;
        return false;
    }

    bool search(const char* s, size_t& mpos, size_t& mlen) const {
        auto l = strlen(s);
        return search(s, l, mpos, mlen);
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
                log(line.first, line.second, "syntax error");
            }
            return r.ret && (!exact || r.len == l);
        }
        return false;
    }

    Definition& operator[](const char* s) {
        return (*grammar_)[s];
    }

    MatchAction match_action;

private:
    std::shared_ptr<Grammar> grammar_;
    std::string              start_;
};

/*-----------------------------------------------------------------------------
 *  Simple interface
 *---------------------------------------------------------------------------*/

struct match
{
    struct Item {
        const char* s;
        size_t      l;
        size_t      id;

        size_t length() const { return l; }
        std::string str() const { return std::string(s, l); }
    };

    std::vector<Item> matches;

    typedef std::vector<Item>::iterator iterator;
    typedef std::vector<Item>::const_iterator const_iterator;

    bool empty() const {
        return matches.empty();
    }

    size_t size() const {
        return matches.size();
    }

    size_t length(size_t n = 0) {
        return matches[n].length();
    }

    std::string str(size_t n = 0) const {
        return matches[n].str();
    }

    const Item& operator[](size_t n) const {
        return matches[n];
    }

    iterator begin() {
        return matches.begin();
    }

    iterator end() {
        return matches.end();
    }

    const_iterator begin() const {
        return matches.cbegin();
    }

    const_iterator end() const {
        return matches.cend();
    }
};

inline bool peg_match(const char* syntax, const char* s, match& m) {
    m.matches.clear();

    peg pg(syntax);
    pg.match_action = [&](const char* s, size_t l, size_t i) {
        m.matches.push_back(match::Item{ s, l, i });
    };

    auto ret = pg.parse(s);
    if (ret) {
        auto l = strlen(s);
        m.matches.insert(m.matches.begin(), match::Item{ s, l, 0 });
    }

    return ret;
}

inline bool peg_match(const char* syntax, const char* s) {
    peg pg(syntax);
    return pg.parse(s);
}

inline bool peg_search(peg& pg, const char* s, size_t l, match& m) {
    m.matches.clear();

    pg.match_action = [&](const char* s, size_t l, size_t i) {
        m.matches.push_back(match::Item{ s, l, i });
    };

    size_t mpos, mlen;
    auto ret = pg.search(s, l, mpos, mlen);
    if (ret) {
        m.matches.insert(m.matches.begin(), match::Item{ s + mpos, mlen, 0 });
        return true;
    }

    return false;
}

inline bool peg_search(peg& pg, const char* s, match& m) {
    auto l = strlen(s);
    return peg_search(pg, s, l, m);
}

inline bool peg_search(const char* syntax, const char* s, size_t l, match& m) {
    peg pg(syntax);
    return peg_search(pg, s, l, m);
}

inline bool peg_search(const char* syntax, const char* s, match& m) {
    peg pg(syntax);
    auto l = strlen(s);
    return peg_search(pg, s, l, m);
}

class peg_token_iterator : public std::iterator<std::forward_iterator_tag, match>
{
public:
    peg_token_iterator()
        : s_(nullptr)
        , l_(0)
        , pos_(std::numeric_limits<size_t>::max()) {}

    peg_token_iterator(const char* syntax, const char* s)
        : peg_(syntax)
        , s_(s)
        , l_(strlen(s))
        , pos_(0) {
        peg_.match_action = [&](const char* s, size_t l, size_t i) {
            m_.matches.push_back(match::Item{ s, l, i });
        };
        search();
    }

    peg_token_iterator(const peg_token_iterator& rhs)
        : peg_(rhs.peg_)
        , s_(rhs.s_)
        , l_(rhs.l_)
        , pos_(rhs.pos_)
        , m_(rhs.m_) {}

    peg_token_iterator& operator++() {
        search();
        return *this;
    }

    peg_token_iterator operator++(int) {
        auto it = *this;
        search();
        return it;
    }

    match& operator*() {
        return m_;
    }

    match* operator->() {
        return &m_;
    }

    bool operator==(const peg_token_iterator& rhs) {
        return pos_ == rhs.pos_;
    }

    bool operator!=(const peg_token_iterator& rhs) {
        return pos_ != rhs.pos_;
    }

private:
    void search() {
        m_.matches.clear();
        size_t mpos, mlen;
        if (peg_.search(s_ + pos_, l_ - pos_, mpos, mlen)) {
            m_.matches.insert(m_.matches.begin(), match::Item{ s_ + mpos, mlen, 0 });
            pos_ += mpos + mlen;
        } else {
            pos_ = std::numeric_limits<size_t>::max();
        }
    }

    peg         peg_;
    const char* s_;
    size_t      l_;
    size_t      pos_;
    match       m_;
};

struct peg_token_range {
    typedef peg_token_iterator iterator;
    typedef const peg_token_iterator const_iterator;

    peg_token_range(const char* syntax, const char* s)
        : beg_iter(peg_token_iterator(syntax, s))
        , end_iter() {}

    iterator begin() {
        return beg_iter;
    }

    iterator end() {
        return end_iter;
    }

    const_iterator cbegin() const {
        return beg_iter;
    }

    const_iterator cend() const {
        return end_iter;
    }

private:
    peg_token_iterator beg_iter;
    peg_token_iterator end_iter;
};


} // namespace peglib

#endif

// vim: et ts=4 sw=4 cin cino={1s ff=unix
