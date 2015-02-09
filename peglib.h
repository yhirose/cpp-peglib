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
#include <iostream>

namespace peglib {

void* enabler;

/*-----------------------------------------------------------------------------
 *  Any
 *---------------------------------------------------------------------------*/

class Any
{
public:
    Any() : content_(nullptr) {
    }

    Any(const Any& rhs) : content_(rhs.clone()) {
    }

    Any(Any&& rhs) : content_(rhs.content_) {
        rhs.content_ = nullptr;
    }

    template <typename T>
    Any(const T& value) : content_(new holder<T>(value)) {
    }

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

    ~Any() {
        delete content_;
    }

    bool is_undefined() const {
        return content_ == nullptr;
    }

    template <typename T>
    T& get() {
        assert(content_);
        return dynamic_cast<holder<T>*>(content_)->value_;
    }

    template <typename T>
    const T& get() const {
        assert(content_);
        return dynamic_cast<holder<T>*>(content_)->value_;
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
 *  Variant
 *---------------------------------------------------------------------------*/

#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
#define static_max(a, b) (a > b ? a : b)
#define alignof _alignof
#else
template <typename T>
constexpr T static_max(T a, T b) { return a > b ? a : b; }
#endif

/*
 * For debug
 */
static int VARINT_COUNT = 0;

template <typename T> void log_copy_construct() {
    VARINT_COUNT++;
}

template <typename T> void log_move_construct() {
    VARINT_COUNT++;
}

template <typename T> void log_destruct() {
    VARINT_COUNT--;
}

void log_variant_count() {
    std::cout << "VARIANT COUNT (" << VARINT_COUNT << ")" << std::endl;
};

/*
 * Type list
 */
template <typename... Ts>
struct typelist;

template <typename T, typename... Ts>
struct typelist<T, Ts...>
{
    static const size_t max_elem_size = static_max(sizeof(T), typelist<Ts...>::max_elem_size);
    static const size_t max_elem_align = static_max(alignof(T), typelist<Ts...>::max_elem_align);
};

template <>
struct typelist<>
{
    static const size_t max_elem_size = 0;
    static const size_t max_elem_align = 0;
};

template <typename T, typename... Ts>
struct typelist_index;

template <typename T, typename U, typename... Us>
struct typelist_index <T, U, Us...>
{
    static const size_t value = 1 + typelist_index<T, Us...>::value;
};

template <typename U, typename... Us>
struct typelist_index <U, U, Us...>
{
    static const size_t value = 0;
};

template <typename U>
struct typelist_index <U>
{
    static const size_t value = 0;
};

/*
 * Variant helper
 */
template <size_t N, typename... Ts>
struct variant_helper;

template <size_t N, typename T, typename... Ts>
struct variant_helper<N, T, Ts...>
{
    template <typename VT>
    static void copy_construct(size_t type_index, void* data, const VT& vt) {
        if (N == type_index) {
            log_copy_construct<T>();
            new (data) T(vt.template get<T>());
            return;
        }
        variant_helper<N + 1, Ts...>::copy_construct(type_index, data, vt);
    }

    template <typename VT>
    static void move_construct(size_t type_index, void* data, VT&& vt) {
        if (N == type_index) {
            log_move_construct<T>();
            new (data) T(std::move(vt.template get<T>()));
            return;
        }
        variant_helper<N + 1, Ts...>::move_construct(type_index, data, vt);
    }

    static void destruct(size_t type_index, void* data) {
        if (N == type_index) {
            log_destruct<T>();
            reinterpret_cast<T*>(data)->~T();
            return;
        }
        variant_helper<N + 1, Ts...>::destruct(type_index, data);
    }
};

template <size_t N>
struct variant_helper<N>
{
    template <typename VT>
    static void copy_construct(size_t type_index, void* data, const VT& vt) {}

    template <typename VT>
    static void move_construct(size_t type_index, void* data, VT&& vt) {}

    static void destruct(size_t type_index, void* data) {}
};


/*
 * Variant
 */
template <typename... Ts>
struct Variant
{
    typedef typelist<Ts...> tlist;
    typedef typename std::aligned_storage<tlist::max_elem_size, tlist::max_elem_align>::type data_type;

    data_type data;
    size_t    type_index;

    template <typename T>
    explicit Variant(const T& val) : type_index(typelist_index<T, Ts...>::value) {
        static_assert(typelist_index<T, Ts...>::value < sizeof...(Ts), "Invalid variant type.");
        log_copy_construct<T>();
        new (&data) T(val);
    }

    template <typename T>
    explicit Variant(T&& val) : type_index(typelist_index<T, Ts...>::value) {
        static_assert(typelist_index<T, Ts...>::value < sizeof...(Ts), "Invalid variant type.");
        log_move_construct<T>();
        new (&data) T(std::move(val));
    }

    Variant() : type_index(sizeof...(Ts)) {}

    Variant(const Variant& rhs) : type_index(rhs.type_index) {
        variant_helper<0, Ts...>::copy_construct(type_index, &data, rhs);
    }

    Variant(Variant&& rhs) : type_index(rhs.type_index) {
        variant_helper<0, Ts...>::move_construct(type_index, &data, rhs);
    }

    Variant& operator=(const Variant& rhs) {
        if (this != &rhs) {
            variant_helper<0, Ts...>::destruct(type_index, &data);
            type_index = rhs.type_index;
            variant_helper<0, Ts...>::copy_construct(type_index, &data, rhs);
        }
        return *this;
    }

    Variant& operator=(Variant&& rhs) {
        if (this != &rhs) {
            variant_helper<0, Ts...>::destruct(type_index, &data);
            type_index = rhs.type_index;
            variant_helper<0, Ts...>::move_construct(type_index, &data, rhs);
        }
        return *this;
    }

    ~Variant() {
        variant_helper<0, Ts...>::destruct(type_index, &data);
    }

    template <typename T>
    T& get() {
        if (type_index != typelist_index<T, Ts...>::value) {
            throw std::invalid_argument("Invalid template argument.");
        }
        return *reinterpret_cast<T*>(&data);
    }

    template <typename T>
    const T& get() const {
        if (type_index != typelist_index<T, Ts...>::value) {
            throw std::invalid_argument("Invalid template argument.");
        }
        return *reinterpret_cast<const T*>(&data);
    }
};

#if _MSC_VER < 1900 // Less than Visual Studio 2015
#undef static_max
#undef alignof
#endif

/*-----------------------------------------------------------------------------
 *  PEG
 *---------------------------------------------------------------------------*/

/*
 * Forward declalations
 */
class Rule;
class Definition;

template <typename T>
struct SemanticActions;

template <typename T>
struct SemanticValues;

/*
 * Match
 */
struct Match
{
    Match(bool _ret, size_t _len) : ret(_ret), len(_len) {}
    bool   ret;
    size_t len;
};

Match success(size_t len) {
   return Match(true, len);
}

Match fail() {
   return Match(false, 0);
}

/*
 * Rules
 */
class Sequence
{
public:
    Sequence(const Sequence& rhs) : rules_(rhs.rules_) {}

#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    // NOTE: Compiler Error C2797 on Visual Studio 2013
    // "The C++ compiler in Visual Studio does not implement list
    // initialization inside either a member initializer list or a non-static
    // data member initializer. Before Visual Studio 2013 Update 3, this was
    // silently converted to a function call, which could lead to bad code
    // generation. Visual Studio 2013 Update 3 reports this as an error."
    template <typename... Args>
    Sequence(const Args& ...args) {
        rules_ = std::vector<std::shared_ptr<Rule>>{ static_cast<std::shared_ptr<Rule>>(args)... };
    }
#else
    template <typename... Args>
    Sequence(const Args& ...args) : rules_{ static_cast<std::shared_ptr<Rule>>(args)... } {}
#endif

    Sequence(const std::vector<std::shared_ptr<Rule>>& rules) : rules_(rules) {}
    Sequence(std::vector<std::shared_ptr<Rule>>&& rules) : rules_(std::move(rules)) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::vector<std::shared_ptr<Rule>> rules_;
};

class PrioritizedChoice
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
        rules_ = std::vector<std::shared_ptr<Rule>>{ static_cast<std::shared_ptr<Rule>>(args)... };
    }
#else
    template <typename... Args>
    PrioritizedChoice(const Args& ...args) : rules_{ static_cast<std::shared_ptr<Rule>>(args)... } {}
#endif

    PrioritizedChoice(const std::vector<std::shared_ptr<Rule>>& rules) : rules_(rules) {}
    PrioritizedChoice(std::vector<std::shared_ptr<Rule>>&& rules) : rules_(std::move(rules)) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::vector<std::shared_ptr<Rule>> rules_;
};

class ZeroOrMore
{
public:
    ZeroOrMore(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule> rule_;
};

class OneOrMore
{
public:
    OneOrMore(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule> rule_;
};

class Option
{
public:
    Option(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule> rule_;
};

class AndPredicate
{
public:
    AndPredicate(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule> rule_;
};

class NotPredicate
{
public:
    NotPredicate(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule> rule_;
};

class LiteralString
{
public:
    LiteralString(const char* s) : lit_(s) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
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

class CharacterClass
{
public:
    CharacterClass(const char* chars) : chars_(chars) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
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

class Character
{
public:
    Character(char ch) : ch_(ch) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
        if (l < 1 || s[0] != ch_) {
            return fail();
        }
        return success(1);
    }

private:
    char ch_;
};

class AnyCharacter
{
public:
    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
        if (l < 1) {
            return fail();
        }
        return success(1);
    }
};

class Grouping
{
public:
    Grouping(const std::shared_ptr<Rule>& rule) : rule_(rule) {}
    Grouping(const std::shared_ptr<Rule>& rule, std::function<void(const char* s, size_t l)> match) : rule_(rule), match_(match) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::shared_ptr<Rule>                        rule_;
    std::function<void(const char* s, size_t l)> match_;
};

class NonTerminal
{
public:
    NonTerminal(Definition* outer) : outer_(outer) {};

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    friend class Definition;

    template<typename T, typename Action>
    T reduce(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n, Action action) const;

    std::shared_ptr<Rule> rule_;
    Definition*           outer_;
};

class DefinitionReference
{
public:
    DefinitionReference(
        const std::map<std::string, Definition>& grammar, const std::string& name)
        : grammar_(grammar)
        , name_(name) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    const std::map<std::string, Definition>& grammar_;
    std::string name_;
};

class WeakHolder
{
public:
    WeakHolder(const std::shared_ptr<Rule>& rule) : weak_(rule) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const;

private:
    std::weak_ptr<Rule> weak_;
};

/*
 * Rule
 */
template <typename... Ts>
class TRule
{
public:
    template <typename T>
    TRule(const T& val) : vt(val) {}

    template <typename T>
    TRule(T&& val) : vt(std::move(val)) {}

    template <typename T>
    Match parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
        switch (vt.type_index) {
            case 0: return vt.template get<Sequence>().template parse<T>(s, l, sa, sv);
            case 1: return vt.template get<PrioritizedChoice>().template parse<T>(s, l, sa, sv);
            case 2: return vt.template get<ZeroOrMore>().template parse<T>(s, l, sa, sv);
            case 3: return vt.template get<OneOrMore>().template parse<T>(s, l, sa, sv);
            case 4: return vt.template get<Option>().template parse<T>(s, l, sa, sv);
            case 5: return vt.template get<AndPredicate>().template parse<T>(s, l, sa, sv);
            case 6: return vt.template get<NotPredicate>().template parse<T>(s, l, sa, sv);
            case 7: return vt.template get<LiteralString>().template parse<T>(s, l, sa, sv);
            case 8: return vt.template get<CharacterClass>().template parse<T>(s, l, sa, sv);
            case 9: return vt.template get<Character>().template parse<T>(s, l, sa, sv);
            case 10: return vt.template get<AnyCharacter>().template parse<T>(s, l, sa, sv);
            case 11: return vt.template get<Grouping>().template parse<T>(s, l, sa, sv);
            case 12: return vt.template get<NonTerminal>().template parse<T>(s, l, sa, sv);
            case 13: return vt.template get<DefinitionReference>().template parse<T>(s, l, sa, sv);
            case 14: return vt.template get<WeakHolder>().template parse<T>(s, l, sa, sv);
        }

        throw std::logic_error("couldn't find the internal data in the variant...");
        return fail();
    }

    Variant<Ts...> vt;
};

class Rule : public TRule<
    Sequence,
    PrioritizedChoice,
    ZeroOrMore,
    OneOrMore,
    Option,
    AndPredicate,
    NotPredicate,
    LiteralString,
    CharacterClass,
    Character,
    AnyCharacter,
    Grouping,
    NonTerminal,
    DefinitionReference,
    WeakHolder>
{
public:
    template <typename T>
    Rule(const T& val) : TRule(val) {}

    template <typename T>
    Rule(T&& val) : TRule(std::move(val)) {}
};

/*
 * Semantic action
 */
template <
    typename T, typename R, typename F,
    typename std::enable_if<!std::is_void<R>::value>::type*& = enabler,
    typename... Args>
T call(F fn, Args&&... args) {
    return T(fn(std::forward<Args>(args)...));
}

template <
    typename T, typename R, typename F,
    typename std::enable_if<std::is_void<R>::value>::type*& = enabler,
    typename... Args>
T call(F fn, Args&&... args) {
    fn(std::forward<Args>(args)...);
    return T();
}

template <typename T>
class SemanticActionAdaptor
{
public:
    operator bool() const {
        return (bool)fn_;
    }

    T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) const {
        return fn_(s, l, v, n);
    }

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, &F::operator());
    }

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, fn);
    }

private:
    template <typename R>
    struct TypeAdaptor {
        TypeAdaptor(std::function<R (const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n)> fn)
            : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_, s, l, v, n);
        }
        std::function<R (const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l_v {
        TypeAdaptor_s_l_v(std::function<R (const char* s, size_t l, const std::vector<T>& v)> fn)
            : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_, s, l, v);
        }
        std::function<R (const char* s, size_t l, const std::vector<T>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_s_l {
        TypeAdaptor_s_l(std::function<R (const char* s, size_t l)> fn) : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_, s, l);
        }
        std::function<R (const char* s, size_t l)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v_n {
        TypeAdaptor_v_n(std::function<R (const std::vector<T>& v, const std::vector<std::string>& n)> fn) : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_, v, n);
        }
        std::function<R (const std::vector<T>& v, const std::vector<std::string>& n)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_v {
        TypeAdaptor_v(std::function<R (const std::vector<T>& v)> fn) : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_, v);
        }
        std::function<R (const std::vector<T>& v)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_empty {
        TypeAdaptor_empty(std::function<R ()> fn) : fn_(fn) {}
        T operator()(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n) {
            return call<T, R>(fn_);
        }
        std::function<R ()> fn_;
    };

    typedef std::function<T(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n)> Fty;

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<T>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<T>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const char*, size_t, const std::vector<T>& v) const) {
        return TypeAdaptor_s_l_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(*mf)(const char*, size_t, const std::vector<T>& v)) {
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
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<T>& v, const std::vector<std::string>& n) const) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<T>& v, const std::vector<std::string>& n)) {
        return TypeAdaptor_v_n<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::*mf)(const std::vector<T>& v) const) {
        return TypeAdaptor_v<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (*mf)(const std::vector<T>& v)) {
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
 * Semantic actions
 */
template <typename T>
struct SemanticActions : public std::map<void*, SemanticActionAdaptor<T>>
{
    typedef T value_type;
};

template <typename T>
struct SemanticValues
{
    std::vector<std::string> names;
    std::vector<T>           values;
};

/*
 * Definition
 */
class Definition
{
public:
    Definition() : rule_(std::make_shared<Rule>(NonTerminal(this))) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , match(rhs.match)
        , rule_(rhs.rule_)
    {
        auto& nt = rule_->vt.get<NonTerminal>();
        nt.outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , match(std::move(rhs.match))
        , rule_(std::move(rhs.rule_))
    {
        auto& nt = rule_->vt.get<NonTerminal>();
        nt.outer_ = this;
    }

    Definition(const std::shared_ptr<Rule>& rule)
        : rule_(std::make_shared<Rule>(NonTerminal(this)))
    {
        set_rule(rule);
    }

    operator std::shared_ptr<Rule>() {
        return std::make_shared<Rule>(WeakHolder(rule_));
    }

    operator void*() {
        return static_cast<void*>(this);
    }

    Definition& operator<=(const std::shared_ptr<Rule>& rule) {
        set_rule(rule);
        return *this;
    }

    template <typename T>
    bool parse(const char* s, size_t l, const SemanticActions<T>& sa, T& val) const {
        SemanticValues<T> sv;

        auto m = rule_->parse<T>(s, l, &sa, &sv);
        auto ret = m.ret && m.len == l;

        if (ret && !sv.values.empty()) {
            val = sv.values[0];
        }

        return ret;
    }

    template <typename T>
    bool parse(const char* s, const SemanticActions<T>& sa, T& val) const {
        return parse(s, strlen(s), sa, val);
    }

    template <typename T>
    bool parse(const char* s, size_t l, T& val) const {
        SemanticValues<Any> sv;
        auto m = rule_->parse<Any>(s, l, nullptr, &sv);
        return m.ret && m.len == l;
    }

    template <typename T>
    bool parse(const char* s, T& val) const {
        return parse(s, strlen(s), val);
    }

    bool parse(const char* s, size_t l) const {
        SemanticValues<Any> sv;
        auto m = rule_->parse<Any>(s, l, nullptr, &sv);
        return m.ret && m.len == l;
    }

    bool parse(const char* s) const {
        return parse(s, strlen(s));
    }

    std::string name;

    std::function<void (const char* s, size_t l)> match;

    template <typename F>
    void operator,(F fn) {
        action = fn;
    }
    SemanticActionAdaptor<Any> action;

private:
    friend class DefinitionReference;

    Definition& operator=(const Definition& rhs);
    Definition& operator=(Definition&& rhs);

    void set_rule(const std::shared_ptr<Rule>& rule) {
        rule_->vt.get<NonTerminal>().rule_ = rule;
    }

    std::shared_ptr<Rule> rule_;
};

/*
 * Implementation
 */
template <typename T>
Match Sequence::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    size_t i = 0;
    for (const auto& rule : rules_) {
        auto m = rule->parse<T>(s + i, l - i, sa, sv);
        if (!m.ret) {
            return fail();
        }
        i += m.len;
    }
    return success(i);
}

template <typename T>
Match PrioritizedChoice::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    for (const auto& rule : rules_) {
        auto m = rule->parse<T>(s, l, sa, sv);
        if (m.ret) {
            return success(m.len);
        }
    }
    return fail();
}

template <typename T>
Match ZeroOrMore::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto i = 0;
    while (l - i > 0) {
        auto m = rule_->parse<T>(s + i, l - i, sa, sv);
        if (!m.ret) {
            break;
        }
        i += m.len;
    }
    return success(i);
}

template <typename T>
Match OneOrMore::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto m = rule_->parse<T>(s, l, sa, sv);
    if (!m.ret) {
        return fail();
    }
    auto i = m.len;
    while (l - i > 0) {
        auto m = rule_->parse<T>(s + i, l - i, sa, sv);
        if (!m.ret) {
            break;
        }
        i += m.len;
    }
    return success(i);
}

template <typename T>
Match Option::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto m = rule_->parse<T>(s, l, sa, sv);
    return success(m.ret ? m.len : 0);
}

template <typename T>
Match AndPredicate::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto m = rule_->parse<T>(s, l, sa, sv);
    if (m.ret) {
        return success(0);
    } else {
        return fail();
    }
}

template <typename T>
Match NotPredicate::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto m = rule_->parse<T>(s, l, sa, sv);
    if (m.ret) {
        return fail();
    } else {
        return success(0);
    }
}

template <typename T>
Match Grouping::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    assert(rule_);
    auto m = rule_->parse<T>(s, l, sa, sv);
    if (m.ret && match_) {
        match_(s, m.len);
    }
    return m;
}

template <typename T>
Match NonTerminal::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    if (!rule_) {
        throw std::logic_error("Uninitialized definition rule was used...");
    }

    std::unique_ptr<SemanticValues<T>> chldsv;
    if (sv) {
        chldsv.reset(new SemanticValues<T>());
    }

    auto m = rule_->parse<T>(s, l, sa, chldsv.get());
    if (m.ret) {
        if (outer_->match) {
            outer_->match(s, m.len);
        }

        if (sv) {
            typedef std::function<T (const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n)> Action;
            Action action;

            if (outer_->action) {
                action = outer_->action;
            } else if (sa) {
                auto it = sa->find(outer_);
                if (it != sa->end()) {
                    action = it->second;
                }
            }

            sv->names.push_back(outer_->name);
            auto val = reduce<T>(s, m.len, chldsv->values, chldsv->names, action);
            sv->values.push_back(val);
        }
    }

    return m;
}

template<typename T, typename Action>
T NonTerminal::reduce(const char* s, size_t l, const std::vector<T>& v, const std::vector<std::string>& n, Action action) const {
    if (action) {
        return action(s, l, v, n);
    } else if (v.empty()) {
        return T();
    } else {
        return v.front();
    }
}

template <typename T>
Match WeakHolder::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto rule = weak_.lock();
    assert(rule);
    return rule->parse<T>(s, l, sa, sv);
}

template <typename T>
Match DefinitionReference::parse(const char* s, size_t l, const SemanticActions<T>* sa, SemanticValues<T>* sv) const {
    auto rule = grammar_.at(name_).rule_;
    return rule->parse<T>(s, l, sa, sv);
}

/*
 * Factories
 */
template <typename... Args>
std::shared_ptr<Rule> seq(Args&& ...args) {
    return std::make_shared<Rule>(Sequence(static_cast<std::shared_ptr<Rule>>(args)...));
}

inline std::shared_ptr<Rule> seq_v(const std::vector<std::shared_ptr<Rule>>& rules) {
    return std::make_shared<Rule>(Sequence(rules));
}

inline std::shared_ptr<Rule> seq_v(std::vector<std::shared_ptr<Rule>>&& rules) {
    return std::make_shared<Rule>(Sequence(std::move(rules)));
}

template <typename... Args>
std::shared_ptr<Rule> cho(Args&& ...args) {
    return std::make_shared<Rule>(PrioritizedChoice(static_cast<std::shared_ptr<Rule>>(args)...));
}

inline std::shared_ptr<Rule> cho_v(const std::vector<std::shared_ptr<Rule>>& rules) {
    return std::make_shared<Rule>(PrioritizedChoice(rules));
}

inline std::shared_ptr<Rule> cho_v(std::vector<std::shared_ptr<Rule>>&& rules) {
    return std::make_shared<Rule>(PrioritizedChoice(std::move(rules)));
}

inline std::shared_ptr<Rule> zom(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(ZeroOrMore(rule));
}

inline std::shared_ptr<Rule> oom(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(OneOrMore(rule));
}

inline std::shared_ptr<Rule> opt(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(Option(rule));
}

inline std::shared_ptr<Rule> apd(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(AndPredicate(rule));
}

inline std::shared_ptr<Rule> npd(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(NotPredicate(rule));
}

inline std::shared_ptr<Rule> lit(const char* lit) {
    return std::make_shared<Rule>(LiteralString(lit));
}

inline std::shared_ptr<Rule> cls(const char* chars) {
    return std::make_shared<Rule>(CharacterClass(chars));
}

inline std::shared_ptr<Rule> chr(char c) {
    return std::make_shared<Rule>(Character(c));
}

inline std::shared_ptr<Rule> any() {
    return std::make_shared<Rule>(AnyCharacter());
}

inline std::shared_ptr<Rule> grp(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Rule>(Grouping(rule));
}

inline std::shared_ptr<Rule> grp(const std::shared_ptr<Rule>& rule, std::function<void (const char* s, size_t l)> match) {
    return std::make_shared<Rule>(Grouping(rule, match));
}

inline std::shared_ptr<Rule> ref(const std::map<std::string, Definition>& grammar, const std::string& name) {
    return std::make_shared<Rule>(DefinitionReference(grammar, name));
}

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

typedef std::map<std::string, Definition> Grammar;

Grammar make_peg_grammar()
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

std::shared_ptr<Grammar> make_grammar(const char* syntax, std::string& start)
{
    Grammar peg = make_peg_grammar();

    // Prepare output variables
    auto grammar = std::make_shared<Grammar>();
    start.clear();

    // Setup actions
    SemanticActions<Any> sa;
    std::set<std::string> refs;

    sa[peg["Definition"]] = [&](const std::vector<Any>& v) {
        const auto& name = v[0].get<std::string>();
        (*grammar)[name] <= v[2].get<std::shared_ptr<Rule>>();
        (*grammar)[name].name = name;

        if (start.empty()) {
            start = name;
        }
    };

    sa[peg["Expression"]] = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Rule>> rules;
        for (auto i = 0u; i < v.size(); i++) {
            if (!(i % 2)) {
                rules.push_back(v[i].get<std::shared_ptr<Rule>>());
            }
        }
        return cho_v(rules);
    };

    sa[peg["Sequence"]] = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Rule>> rules;
        for (const auto& x: v) {
            rules.push_back(x.get<std::shared_ptr<Rule>>());
        }
        return seq_v(rules);
    };

    sa[peg["Prefix"]] = [&](const std::vector<Any>& v, const std::vector<std::string>& n) {
        std::shared_ptr<Rule> rule;
        if (v.size() == 1) {
            rule = v[0].get<std::shared_ptr<Rule>>();
        } else {
            assert(v.size() == 2);
            rule = v[1].get<std::shared_ptr<Rule>>();
            if (n[0] == "AND") {
                rule = apd(rule);
            } else { // "NOT"
                rule = npd(rule);
            }
        }
        return rule;
    };

    sa[peg["Suffix"]] = [&](const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
        auto rule = v[0].get<std::shared_ptr<Rule>>();
        if (v.size() == 1) {
            return rule;
        } else {
            assert(v.size() == 2);
            if (n[1] == "QUESTION") {
                return opt(rule);
            } else if (n[1] == "STAR") {
                return zom(rule);
            } else { // "PLUS"
                return oom(rule);
            }
        }
    };

    sa[peg["Primary"]] = [&](const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
        if (v.size() == 3) {
            return v[1];
        } else if (n[0] == "Identifier") {
            const auto& name = v[0].get<std::string>();
            refs.insert(name);
            const Any rule(ref(*grammar, name));
            return rule;
        } else {
            return v[0];
        }
    };

    sa[peg["IdentCont"]] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    sa[peg["Literal"]] = [](const std::vector<Any>& v) {
        return lit(v[0].get<std::string>().c_str());
    };
    sa[peg["SQCont"]] = [](const char*s, size_t l) {
        return std::string(s, l);
    };
    sa[peg["DQCont"]] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    sa[peg["Class"]] = [](const std::vector<Any>& v) {
        return cls(v[0].get<std::string>().c_str());
    };
    sa[peg["ClassCont"]] = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    sa[peg["DOT"]] = []() {
        return any();
    };

    Any val;
    if (peg["Grammar"].parse(syntax, sa, val)) {
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
            return (*grammar_)[start_].parse(s, actions_, val);
        return false;
    }

    bool parse(const char* s) const {
        if (grammar_ != nullptr) {
            Any val;
            return (*grammar_)[start_].parse(s, actions_, val);
        }
        return false;
    }

    template <typename T>
    bool parse(const char* s, T& out) const {
        if (grammar_ != nullptr) {
            Any val;
            auto ret = (*grammar_)[start_].parse(s, actions_, val);
            if (ret) {
                out = val.get<T>();
            }
            return ret;
        }
        return false;
    }

    SemanticActionAdaptor<Any>& operator[](const char* s) {
        return actions_[(*grammar_)[s]];
    }

private:
    std::shared_ptr<Grammar> grammar_;
    std::string              start_;
    SemanticActions<Any>     actions_;
};

Parser make_parser(const char* syntax) {
    Parser parser;
    if (!parser.load_syntax(syntax)) {
        throw std::logic_error("PEG syntax error.");
    }
    return parser;
}

} // namespace peglib

#endif

// vim: et ts=4 sw=4 cin cino={1s ff=unix
