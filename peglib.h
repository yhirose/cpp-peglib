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
#include <mutex>
#include <vector>
#include <unordered_map>
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
    SemanticValue()
        : s(nullptr), l(0) {}

    SemanticValue(const any& _val, const char* _name, const char* _s, size_t _l)
        : val(_val), name(_name), s(_s), l(_l) {}

    template <typename T>
    T& get() {
        return val.get<T>();
    }

    template <typename T>
    const T& get() const {
        return val.get<T>();
    }

    std::string str() const {
        return std::string(s, l);
    }

    any         val;
    const char* name;
    const char* s;
    size_t      l;
};

struct SemanticValues : protected std::vector<SemanticValue>
{
    const char* s;
    size_t      l;
    size_t      choice;

    SemanticValues() : s(nullptr), l(0), choice(0) {}

    std::string str(size_t i = 0) const {
        if (i > 0) {
            return (*this)[i].str();
        }
        return std::string(s, l);
    }

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
    using std::vector<T>::emplace;
    using std::vector<T>::emplace_back;

    template <typename T, typename U, typename F>
    static U reduce(T i, T end, U val, F f) {
        if (i == end) {
            return val;
        }
        std::tie(val, i) = f(val, i);
        return reduce(i, end, val, f);
    };

    template <typename F>
    auto map(F f) const -> vector<typename std::remove_const<decltype(f(SemanticValue()))>::type> {
        vector<typename std::remove_const<decltype(f(SemanticValue()))>::type> r;
        for (const auto& v: *this) {
            r.push_back(f(v));
        }
        return r;
    }

    template <typename It, typename F>
    auto map(It beg, It end, F f) const -> vector<typename std::remove_const<decltype(f(SemanticValue()))>::type> {
        vector<typename std::remove_const<decltype(f(SemanticValue()))>::type> r;
        auto it = beg;
        while (it != end) {
            r.push_back(f(*it));
            ++it;
        }
        return r;
    }

    template <typename T>
    auto map() const -> vector<T> {
        return this->map([](const SemanticValue& v) { return v.get<T>(); });
    }

    template <typename T, typename It>
    auto map(It beg, It end) const -> vector<T> {
        return this->map(beg, end, [](const SemanticValue& v) { return v.get<T>(); });
    }
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
    typename std::enable_if<std::is_same<typename std::remove_cv<R>::type, any>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return fn(std::forward<Args>(args)...);
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_same<typename std::remove_cv<R>::type, SemanticValue>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return fn(std::forward<Args>(args)...).val;
}

template <
    typename R, typename F,
    typename std::enable_if<
        !std::is_void<R>::value &&
        !std::is_same<typename std::remove_cv<R>::type, any>::value &&
        !std::is_same<typename std::remove_cv<R>::type, SemanticValue>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return any(fn(std::forward<Args>(args)...));
}

class Action
{
public:
    Action() = default;

    Action(const Action& rhs) : fn_(rhs.fn_) {}

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
typedef std::function<void (const char* s, size_t l, size_t id, const std::string& name)> MatchAction;

/*
 * Result
 */
bool success(int len) {
    return len != -1;
}

bool fail(int len) {
    return len == -1;
}

/*
 * Context
 */
struct Context
{
    const char*                                  s;
    size_t                                       l;

    const char*                                  error_ptr;
    const char*                                  msg; // TODO: should be `int`.

    size_t                                       def_count;
    std::vector<bool>                            cache_register;
    std::vector<bool>                            cache_success;

    std::map<std::pair<size_t, size_t>, std::tuple<int, any>> cache_result;

    std::vector<std::shared_ptr<SemanticValues>> stack;
    size_t                                       stack_size;

    Context(const char* _s, size_t _l, size_t _def_count, bool enablePackratParsing)
        : s(_s)
        , l(_l)
        , def_count(_def_count)
        , cache_register(enablePackratParsing ? def_count * (l + 1) : 0)
        , cache_success(enablePackratParsing ? def_count * (l + 1) : 0)
        , stack_size(0)
    {
    }

    template <typename T>
    void packrat(const char* s, size_t def_id, int& len, any& val, T fn) {
        if (cache_register.empty()) {
            fn(len, val);
            return;
        }

        auto col = s - this->s;
        auto has_cache = cache_register[def_count * col + def_id];

        if (has_cache) {
            if (cache_success[def_count * col + def_id]) {
                const auto& key = std::make_pair((int)(s - this->s), def_id);
                std::tie(len, val) = cache_result[key];
                return;
            } else {
                len = -1;
                return;
            }
        } else {
            fn(len, val);
            cache_register[def_count * col + def_id] = true;
            cache_success[def_count * col + def_id] = success(len);
            if (success(len)) {
                const auto& key = std::make_pair((int)(s - this->s), def_id);
                cache_result[key] = std::make_pair(len, val);
            }
            return;
        }
    }

    inline SemanticValues& push() {
        assert(stack_size <= stack.size());
        if (stack_size == stack.size()) {
            stack.push_back(std::make_shared<SemanticValues>());
        }
        auto& sv = *stack[stack_size++];
        if (!sv.empty()) {
            sv.clear();
        }
        sv.s = nullptr;
        sv.l = 0;
        return sv;
    }

    void pop() {
        stack_size--;
    }
};

/*
 * Parser operators
 */
class Ope
{
public:
    struct Visitor;

    virtual ~Ope() {};
    virtual int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const = 0;
    virtual void accept(Visitor& v) = 0;
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

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        size_t i = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            auto len = rule.parse(s + i, l - i, sv, c, dt);
            if (fail(len)) {
                if (!c.msg) {
                    c.msg = "missing an element in the 'sequence'";
                }
                return -1;
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

//private:
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

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        size_t id = 0;
        for (const auto& ope : opes_) {
            const auto& rule = *ope;
            //SemanticValues chldsv;
            auto& chldsv = c.push();
            auto len = rule.parse(s, l, chldsv, c, dt);
            if (len != -1) {
                if (!chldsv.empty()) {
                    sv.insert(sv.end(), chldsv.begin(), chldsv.end());
                }
                sv.s = chldsv.s;
                sv.l = chldsv.l;
                sv.choice = id;
                c.pop();
                return len;
            }
            id++;
            c.pop();
        }
        c.error_ptr = s;
        c.msg = "nothing was matched in the 'prioritized choice'";
        return -1;
    }

    void accept(Visitor& v) override;

    size_t size() const { return opes_.size();  }

//private:
    std::vector<std::shared_ptr<Ope>> opes_;
};

class ZeroOrMore : public Ope
{
public:
    ZeroOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        auto i = 0;
        while (l - i > 0) {
            const auto& rule = *ope_;
            auto len = rule.parse(s + i, l - i, sv, c, dt);
            if (fail(len)) {
                break;
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

class OneOrMore : public Ope
{
public:
    OneOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        if (fail(len)) {
            if (!c.msg) {
                c.msg = "nothing occurred in the 'one-or-more'";
            }
            return -1;
        }
        auto i = len;
        while (l - i > 0) {
            const auto& rule = *ope_;
            auto len = rule.parse(s + i, l - i, sv, c, dt);
            if (fail(len)) {
                break;
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

class Option : public Ope
{
public:
    Option(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        return success(len) ? len : 0;
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

class AndPredicate : public Ope
{
public:
    AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        if (success(len)) {
            return 0;
        } else {
            return -1;
        }
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope
{
public:
    NotPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        if (success(len)) {
            c.error_ptr = s;
            return -1;
        } else {
            return 0;
        }
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

class LiteralString : public Ope
{
public:
    LiteralString(const std::string& s) : lit_(s) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        auto i = 0u;
        for (; i < lit_.size(); i++) {
            if (i >= l || s[i] != lit_[i]) {
                c.error_ptr = s;
                return -1;
            }
        }
        return i;
    }

    void accept(Visitor& v) override;

//private:
    std::string lit_;
};

class CharacterClass : public Ope
{
public:
    CharacterClass(const std::string& chars) : chars_(chars) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        // TODO: UTF8 support
        if (l < 1) {
            c.error_ptr = s;
            return -1;
        }
        auto ch = s[0];
        auto i = 0u;
        while (i < chars_.size()) {
            if (i + 2 < chars_.size() && chars_[i + 1] == '-') {
                if (chars_[i] <= ch && ch <= chars_[i + 2]) {
                    return 1;
                }
                i += 3;
            } else {
                if (chars_[i] == ch) {
                    return 1;
                }
                i += 1;
            }
        }
        c.error_ptr = s;
        return -1;
    }

    void accept(Visitor& v) override;

//private:
    std::string chars_;
};

class Character : public Ope
{
public:
    Character(char ch) : ch_(ch) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        // TODO: UTF8 support
        if (l < 1 || s[0] != ch_) {
            c.error_ptr = s;
            return -1;
        }
        return 1;
    }

    void accept(Visitor& v) override;

//private:
    char ch_;
};

class AnyCharacter : public Ope
{
public:
    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        // TODO: UTF8 support
        if (l < 1) {
            c.error_ptr = s;
            return -1;
        }
        return 1;
    }

    void accept(Visitor& v) override;
};

class Capture : public Ope
{
public:
    Capture(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t n, const std::string& s)
        : ope_(ope), match_action_(ma), id(n), name(s) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        assert(ope_);
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        if (success(len) && match_action_) {
            match_action_(s, len, id, name);
        }
        return len;
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
    MatchAction          match_action_;
    size_t               id;
    std::string          name;
};

class Anchor : public Ope
{
public:
    Anchor(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        assert(ope_);
        const auto& rule = *ope_;
        auto len = rule.parse(s, l, sv, c, dt);
        if (success(len)) {
            sv.s = s;
            sv.l = len;
        }
        return len;
    }

    void accept(Visitor& v) override;

//private:
    std::shared_ptr<Ope> ope_;
};

typedef std::function<int (const char* s, size_t l, SemanticValues& sv, any& dt)> Parser;

class User : public Ope
{
public:
    User(Parser fn) : fn_(fn) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        assert(fn_);
        return fn_(s, l, sv, dt);
    }

    void accept(Visitor& v) override;

//private:
    std::function<int (const char* s, size_t l, SemanticValues& sv, any& dt)> fn_;
};

class WeakHolder : public Ope
{
public:
    WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override {
        auto ope = weak_.lock();
        assert(ope);
        const auto& rule = *ope;
        return rule.parse(s, l, sv, c, dt);
    }

    void accept(Visitor& v) override;

//private:
    std::weak_ptr<Ope> weak_;
};

class Definition;

class Holder : public Ope
{
public:
    Holder(Definition* outer)
       : outer_(outer) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

//private:
    friend class Definition;

    any reduce(const SemanticValues& sv, any& dt, const Action& action) const;

    std::shared_ptr<Ope> ope_;
    Definition*          outer_;
};

class DefinitionReference : public Ope
{
public:
    DefinitionReference(
        const std::unordered_map<std::string, Definition>& grammar, const std::string& name)
        : grammar_(grammar)
        , name_(name) {}

    int parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> get_rule() const;

private:
    const std::unordered_map<std::string, Definition>& grammar_;
    const std::string                                  name_;
    mutable std::once_flag                             init_;
    mutable std::shared_ptr<Ope>                       rule_;
};

/*
 * Visitor
 */
struct Ope::Visitor
{
    virtual void visit(Sequence& ope) = 0;
    virtual void visit(PrioritizedChoice& ope) = 0;
    virtual void visit(ZeroOrMore& ope) = 0;
    virtual void visit(OneOrMore& ope) = 0;
    virtual void visit(Option& ope) = 0;
    virtual void visit(AndPredicate& ope) = 0;
    virtual void visit(NotPredicate& ope) = 0;
    virtual void visit(LiteralString& ope) = 0;
    virtual void visit(CharacterClass& ope) = 0;
    virtual void visit(Character& ope) = 0;
    virtual void visit(AnyCharacter& ope) = 0;
    virtual void visit(Capture& ope) = 0;
    virtual void visit(Anchor& ope) = 0;
    virtual void visit(User& ope) = 0;
    virtual void visit(WeakHolder& ope) = 0;
    virtual void visit(Holder& ope) = 0;
    virtual void visit(DefinitionReference& ope) = 0;
};

struct DefinitionIDs : public Ope::Visitor
{
    void visit(Sequence& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(PrioritizedChoice& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(ZeroOrMore& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(OneOrMore& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(Option& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(AndPredicate& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(NotPredicate& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(LiteralString& ope) override {
    }
    void visit(CharacterClass& ope) override {
    }
    void visit(Character& ope) override {
    }
    void visit(AnyCharacter& ope) override {
    }
    void visit(Capture& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(Anchor& ope) override {
        ope.ope_->accept(*this);
    }
    void visit(User& ope) override {
    }
    void visit(WeakHolder& ope) override {
        ope.weak_.lock()->accept(*this);
    }
    void visit(Holder& ope) override;
    void visit(DefinitionReference& ope) override {
        ope.get_rule()->accept(*this);
    }

    std::unordered_map<void*, size_t> ids;
};

/*
 * Definition
 */
class Definition
{
public:
    struct Result {
        bool        ret;
        int         len;
        const char* error_ptr;
        const char* msg;
    };

    Definition()
        : actions(1)
        , ignoreSemanticValue(false)
        , enablePackratParsing(false)
        , holder_(std::make_shared<Holder>(this)) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , actions(1)
        , ignoreSemanticValue(false)
        , enablePackratParsing(false)
        , holder_(rhs.holder_)
    {
        holder_->outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , actions(1)
        , ignoreSemanticValue(rhs.ignoreSemanticValue)
        , enablePackratParsing(rhs.enablePackratParsing)
        , holder_(std::move(rhs.holder_))
    {
        holder_->outer_ = this;
    }

    Definition(const std::shared_ptr<Ope>& ope)
        : actions(1)
        , ignoreSemanticValue(false)
        , enablePackratParsing(false)
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

    Result parse(const char* s, size_t l) const {
        SemanticValues sv;
        any dt;
        return parse_core(s, l, sv, dt);
    }

    Result parse(const char* s) const {
        auto l = strlen(s);
        return parse(s, l);
    }

    Result parse_with_data(const char* s, size_t l, any& dt) const {
        SemanticValues sv;
        return parse_core(s, l, sv, dt);
    }

    Result parse_with_data(const char* s, any& dt) const {
        auto l = strlen(s);
        return parse_with_data(s, l, dt);
    }

    template <typename T>
    Result parse_with_value(const char* s, size_t l, T& val) const {
        SemanticValues sv;
        any dt;
        auto r = parse_core(s, l, sv, dt);
        if (r.ret && !sv.empty() && !sv.front().val.is_undefined()) {
            val = sv[0].val.get<T>();
        }
        return r;
    }

    template <typename T>
    Result parse_with_value(const char* s, T& val) const {
        auto l = strlen(s);
        return parse_with_value(s, l, val);
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
        ignoreSemanticValue = true;
        return *this;
    }

    void accept(Ope::Visitor& v) {
        holder_->accept(v);
    }

    std::string                  name;
    size_t                       id;

    std::vector<Action>          actions;
    std::function<void(any& dt)> before;
    std::function<void(any& dt)> after;

    bool                         ignoreSemanticValue;
    bool                         enablePackratParsing;

private:
    friend class DefinitionReference;

    Definition& operator=(const Definition& rhs);
    Definition& operator=(Definition&& rhs);

    Result parse_core(const char* s, size_t l, SemanticValues& sv, any& dt) const {
        DefinitionIDs defIds;
        holder_->accept(defIds);

        Context c(s, l, defIds.ids.size(), enablePackratParsing);
        auto len = holder_->parse(s, l, sv, c, dt);
        return Result { success(len), len, c.error_ptr, c.msg };
    }

    std::shared_ptr<Holder> holder_;
};

typedef Definition rule;

/*
 * Implementations
 */

inline int Holder::parse(const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const {
    if (!ope_) {
        throw std::logic_error("Uninitialized definition ope was used...");
    }

    int len;
    any val;
    const char* ancs = s;
    size_t      ancl = l;

    c.packrat(s, outer_->id, len, val, [&](int& len, any& val) {
        auto& chldsv = c.push();

        if (outer_->before) {
            outer_->before(dt);
        }

        const auto& rule = *ope_;
        len = rule.parse(s, l, chldsv, c, dt);
        ancl = len;
        if (success(len) && !outer_->ignoreSemanticValue) {
            assert(!outer_->actions.empty());

            auto i = chldsv.choice + 1; // Index 0 is for the default action
            const auto& action = (i < outer_->actions.size() && outer_->actions[i])
                ? outer_->actions[i]
                : outer_->actions[0];

            if (chldsv.s) {
                ancs = chldsv.s;
                ancl = chldsv.l;
            } else {
                chldsv.s = s;
                chldsv.l = len;
            }

            val = reduce(chldsv, dt, action);
        }

        if (outer_->after) {
            outer_->after(dt);
        }

        c.pop();
    });

    if (success(len) && !outer_->ignoreSemanticValue) {
        sv.emplace_back(val, outer_->name.c_str(), ancs, ancl);
    }

    return len;
}

inline any Holder::reduce(const SemanticValues& sv, any& dt, const Action& action) const {
    if (action) {
        return action(sv, dt);
    } else if (sv.empty()) {
        return any();
    } else {
        return sv.front().val;
    }
}

inline int DefinitionReference::parse(
    const char* s, size_t l, SemanticValues& sv, Context& c, any& dt) const {
    const auto& rule = *get_rule();
    return rule.parse(s, l, sv, c, dt);
}

inline std::shared_ptr<Ope> DefinitionReference::get_rule() const {
    if (!rule_) {
        std::call_once(init_, [this]() {
            rule_ = grammar_.at(name_).holder_;
        });
    }
    assert(rule_);
    return rule_;
};

inline void Sequence::accept(Visitor& v) { v.visit(*this); }
inline void PrioritizedChoice::accept(Visitor& v) { v.visit(*this); }
inline void ZeroOrMore::accept(Visitor& v) { v.visit(*this); }
inline void OneOrMore::accept(Visitor& v) { v.visit(*this); }
inline void Option::accept(Visitor& v) { v.visit(*this); }
inline void AndPredicate::accept(Visitor& v) { v.visit(*this); }
inline void NotPredicate::accept(Visitor& v) { v.visit(*this); }
inline void LiteralString::accept(Visitor& v) { v.visit(*this); }
inline void CharacterClass::accept(Visitor& v) { v.visit(*this); }
inline void Character::accept(Visitor& v) { v.visit(*this); }
inline void AnyCharacter::accept(Visitor& v) { v.visit(*this); }
inline void Capture::accept(Visitor& v) { v.visit(*this); }
inline void Anchor::accept(Visitor& v) { v.visit(*this); }
inline void User::accept(Visitor& v) { v.visit(*this); }
inline void WeakHolder::accept(Visitor& v) { v.visit(*this); }
inline void Holder::accept(Visitor& v) { v.visit(*this); }
inline void DefinitionReference::accept(Visitor& v) { v.visit(*this); }

inline void DefinitionIDs::visit(Holder& ope) {
    auto p = (void*)ope.outer_;
    if (ids.find(p) != ids.end()) {
        return;
    }
    auto id = ids.size();
    ids[p] = id;
    ope.outer_->id = id;
    ope.ope_->accept(*this);
}

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

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t n, const std::string& s) {
    return std::make_shared<Capture>(ope, ma, n, s);
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma) {
    return std::make_shared<Capture>(ope, ma, (size_t)-1, std::string());
}

inline std::shared_ptr<Ope> anc(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Anchor>(ope);
}

inline std::shared_ptr<Ope> usr(std::function<int (const char* s, size_t l, SemanticValues& sv, any& dt)> fn) {
    return std::make_shared<User>(fn);
}

inline std::shared_ptr<Ope> ref(const std::unordered_map<std::string, Definition>& grammar, const std::string& name) {
    return std::make_shared<DefinitionReference>(grammar, name);
}

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

inline std::pair<size_t, size_t> line_info(const char* beg, const char* cur) {
    auto p = beg;
    auto col_ptr = p;
    auto no = 1;

    while (p < cur) {
        if (*p == '\n') {
            no++;
            col_ptr = p + 1;
        }
        p++;
    }

    auto col = p - col_ptr + 1;

    return std::make_pair(no, col);
}

typedef std::unordered_map<std::string, Definition> Grammar;
typedef std::function<void (size_t, size_t, const std::string&)> Log;

typedef std::unordered_map<std::string, std::shared_ptr<Ope>> Rules;

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

    struct Data {
        std::shared_ptr<Grammar>                     grammar;
        std::string                                  start;
        MatchAction                                  match_action;
        std::unordered_map<std::string, const char*> references;
        size_t                                       capture_count;

        Data() : grammar(std::make_shared<Grammar>()), capture_count(0) {}
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
                               seq(chr('\\'), cls("0-3"), cls("0-7"), cls("0-7")),
                               seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                               seq(lit("\\x"), cls("0-9a-fA-F"), opt(cls("0-9a-fA-F"))),
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

        g["BeginCap"]   <= seq(chr('$'), anc(opt(g["Identifier"])), chr('<'), g["Spacing"]);
        g["EndCap"]     <= seq(lit(">"), g["Spacing"]);

        g["IGNORE"]     <= chr('~');

        // Set definition names
        for (auto& x: g) {
            x.second.name = x.first;
        }
    }

    void setup_actions() {
        g["Definition"] = [&](const SemanticValues& sv, any& dt) {
            Data& data = *dt.get<Data*>();

            auto ignore = (sv.size() == 4);
            auto baseId = ignore ? 1 : 0;

            const auto& name = sv[baseId].val.get<std::string>();
            auto ope = sv[baseId + 2].val.get<std::shared_ptr<Ope>>();

            auto& def = (*data.grammar)[name];
            def <= ope;
            def.name = name;
            def.ignoreSemanticValue = ignore;

            if (data.start.empty()) {
                data.start = name;
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
                Data& data = *dt.get<Data*>();
                const auto& ident = sv[0].val.get<std::string>();
                data.references[ident] = sv.s; // for error handling
                return ref(*data.grammar, ident);
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
                Data& data = *dt.get<Data*>();
                auto name = std::string(sv[0].s, sv[0].l);
                auto ope = sv[1].val.get<std::shared_ptr<Ope>>();
                return cap(ope, data.match_action, ++data.capture_count, name);
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
        Data data;
        data.match_action = ma;

        any dt = &data;
        auto r = g["Grammar"].parse_with_data(s, l, dt);

        if (!r.ret) {
            if (log) {
                auto line = line_info(s, r.error_ptr);
                log(line.first, line.second, r.msg ? "syntax error" : r.msg);
            }
            return nullptr;
        }

        auto& grammar = *data.grammar;

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
                def.ignoreSemanticValue = ignore;
            }
        }

        // Check missing definitions
        for (const auto& x : data.references) {
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

        start = data.start;

        return data.grammar;
    }

    bool is_hex(char c, int& v) {
        if ('0' <= c && c <= '9') {
            v = c - '0';
            return true;
        } else if ('a' <= c && c <= 'f') {
            v = c - 'a' + 10;
            return true;
        } else if ('A' <= c && c <= 'F') {
            v = c - 'A' + 10;
            return true;
        }
        return false;
    }

    bool is_digit(char c, int& v) {
        if ('0' <= c && c <= '9') {
            v = c - '0';
            return true;
        }
        return false;
    }

    std::pair<char, int> parse_hex_number(const char* s, size_t l, size_t i) {
        char ret = 0;
        int n;
        if (i < l && is_hex(s[i], n)) {
            ret = n;
            if (i + 1 < l && is_hex(s[i + 1], n)) {
                ret = ret * 16 + n;
                i++;
            }
        }
        return std::make_pair(ret, i);
    }

    std::pair<char, int> parse_octal_number(const char* s, size_t l, size_t i) {
        char ret = 0;
        int n;
        if (i < l && is_digit(s[i], n)) {
            ret = n;
            if (i + 1 < l && is_digit(s[i + 1], n)) {
                ret = ret * 8 + n;
                i++;
                if (i + 1 < l && is_digit(s[i + 1], n)) {
                    ret = ret * 8 + n;
                    i++;
                }
            }
        }
        return std::make_pair(ret, i);
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
                    case 'x': {
                        std::tie(ch, i) = parse_hex_number(s, l, i + 1);
                        r += ch;
                        break;
                    }
                    default: {
                        std::tie(ch, i) = parse_octal_number(s, l, i + 1);
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
            [&](const char* s, size_t l, size_t id, const std::string& name) {
                if (match_action) match_action(s, l, id, name);
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

    bool parse(const char* s, size_t l, Log log = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, l);
            output_log(s, l, log, r);
            return r.ret && r.len == l;
        }
        return false;
    }

    bool parse(const char* s, Log log = nullptr) const {
        auto l = strlen(s);
        return parse(s, l, log);
    }

    bool parse_with_data(const char* s, size_t l, any& dt, Log log = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse_with_data(s, l, dt);
            output_log(s, l, log, r);
            return r.ret && r.len == l;
        }
        return false;
    }

    bool parse_with_data(const char* s, any& dt, Log log = nullptr) const {
        auto l = strlen(s);
        return parse_with_data(s, l, dt, log);
    }

    template <typename T>
    bool parse_with_value(const char* s, size_t l, T& out, Log log = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse_with_value(s, l, out);
            output_log(s, l, log, r);
            return r.ret && r.len == l;
        }
        return false;
    }

    template <typename T>
    bool parse_with_value(const char* s, T& out, Log log = nullptr) const {
        auto l = strlen(s);
        return parse_with_value(s, l, out, log);
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
                    mlen = len;
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

    Definition& operator[](const char* s) {
        return (*grammar_)[s];
    }

    void packrat_parsing(bool sw) {
        if (grammar_ != nullptr) {
            auto& rule = (*grammar_)[start_];
            rule.enablePackratParsing = sw;
        }
    }

    MatchAction match_action;

private:
    void output_log(const char* s, size_t l, Log log, const Definition::Result& r) const {
        if (log) {
            if (!r.ret) {
                auto line = line_info(s, r.error_ptr);
                log(line.first, line.second, r.msg ? "syntax error" : r.msg);
            } else if (r.len != l) {
                auto line = line_info(s, s + r.len);
                log(line.first, line.second, "syntax error");
            }
        }
    }

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
        std::string name;

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

    std::vector<size_t> named_capture(const std::string& name) const {
        std::vector<size_t> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            if (matches[i].name == name) {
                ret.push_back(i);
            }
        }
        return ret;
    }

    std::map<std::string, std::vector<size_t>> named_captures() const {
        std::map<std::string, std::vector<size_t>> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            ret[matches[i].name].push_back(i);
        }
        return ret;
    }

    std::vector<size_t> indexed_capture(size_t id) const {
        std::vector<size_t> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            if (matches[i].id == id) {
                ret.push_back(i);
            }
        }
        return ret;
    }

    std::map<size_t, std::vector<size_t>> indexed_captures() const {
        std::map<size_t, std::vector<size_t>> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            ret[matches[i].id].push_back(i);
        }
        return ret;
    }
};

inline bool peg_match(const char* syntax, const char* s, match& m) {
    m.matches.clear();

    peg pg(syntax);
    pg.match_action = [&](const char* s, size_t l, size_t id, const std::string& name) {
        m.matches.push_back(match::Item{ s, l, id, name });
    };

    auto ret = pg.parse(s);
    if (ret) {
        auto l = strlen(s);
        m.matches.insert(m.matches.begin(), match::Item{ s, l, 0, std::string() });
    }

    return ret;
}

inline bool peg_match(const char* syntax, const char* s) {
    peg pg(syntax);
    return pg.parse(s);
}

inline bool peg_search(peg& pg, const char* s, size_t l, match& m) {
    m.matches.clear();

    pg.match_action = [&](const char* s, size_t l, size_t id, const std::string& name) {
        m.matches.push_back(match::Item{ s, l, id, name });
    };

    size_t mpos, mlen;
    auto ret = pg.search(s, l, mpos, mlen);
    if (ret) {
        m.matches.insert(m.matches.begin(), match::Item{ s + mpos, mlen, 0, std::string() });
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
        peg_.match_action = [&](const char* s, size_t l, size_t id, const std::string& name) {
            m_.matches.push_back(match::Item{ s, l, id, name });
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
