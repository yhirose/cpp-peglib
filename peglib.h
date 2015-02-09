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
 * Forward declalations
 */
class Definition;

/*
* Semantic values
*/
struct SemanticValues
{
    std::vector<std::string> names;
    std::vector<Any>         values;
};

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
class Rule
{
   public:
      virtual ~Rule() {};
      virtual Match parse(const char* s, size_t l, SemanticValues& sv) const = 0;
};

class Sequence : public Rule
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

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        size_t i = 0;
        for (const auto& rule : rules_) {
            auto m = rule->parse(s + i, l - i, sv);
            if (!m.ret) {
                return fail();
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::vector<std::shared_ptr<Rule>> rules_;
};

class PrioritizedChoice : public Rule
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

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        for (const auto& rule : rules_) {
            auto m = rule->parse(s, l, sv);
            if (m.ret) {
                return success(m.len);
            }
        }
        return fail();
    }


private:
    std::vector<std::shared_ptr<Rule>> rules_;
};

class ZeroOrMore : public Rule
{
public:
    ZeroOrMore(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto i = 0;
        while (l - i > 0) {
            auto m = rule_->parse(s + i, l - i, sv);
            if (!m.ret) {
                break;
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::shared_ptr<Rule> rule_;
};

class OneOrMore : public Rule
{
public:
    OneOrMore(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto m = rule_->parse(s, l, sv);
        if (!m.ret) {
            return fail();
        }
        auto i = m.len;
        while (l - i > 0) {
            auto m = rule_->parse(s + i, l - i, sv);
            if (!m.ret) {
                break;
            }
            i += m.len;
        }
        return success(i);
    }

private:
    std::shared_ptr<Rule> rule_;
};

class Option : public Rule
{
public:
    Option(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto m = rule_->parse(s, l, sv);
        return success(m.ret ? m.len : 0);
    }

private:
    std::shared_ptr<Rule> rule_;
};

class AndPredicate : public Rule
{
public:
    AndPredicate(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto m = rule_->parse(s, l, sv);
        if (m.ret) {
            return success(0);
        } else {
            return fail();
        }
    }

private:
    std::shared_ptr<Rule> rule_;
};

class NotPredicate : public Rule
{
public:
    NotPredicate(const std::shared_ptr<Rule>& rule) : rule_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto m = rule_->parse(s, l, sv);
        if (m.ret) {
            return fail();
        } else {
            return success(0);
        }
    }

private:
    std::shared_ptr<Rule> rule_;
};

class LiteralString : public Rule
{
public:
    LiteralString(const char* s) : lit_(s) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
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

class CharacterClass : public Rule
{
public:
    CharacterClass(const char* chars) : chars_(chars) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
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

class Character : public Rule
{
public:
    Character(char ch) : ch_(ch) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        if (l < 1 || s[0] != ch_) {
            return fail();
        }
        return success(1);
    }

private:
    char ch_;
};

class AnyCharacter : public Rule
{
public:
    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        if (l < 1) {
            return fail();
        }
        return success(1);
    }
};

class Grouping : public Rule
{
public:
    Grouping(const std::shared_ptr<Rule>& rule) : rule_(rule) {}
    Grouping(const std::shared_ptr<Rule>& rule, std::function<void(const char* s, size_t l)> match) : rule_(rule), match_(match) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        assert(rule_);
        auto m = rule_->parse(s, l, sv);
        if (m.ret && match_) {
            match_(s, m.len);
        }
        return m;
    }

private:
    std::shared_ptr<Rule>                        rule_;
    std::function<void(const char* s, size_t l)> match_;
};

class WeakHolder : public Rule
{
public:
    WeakHolder(const std::shared_ptr<Rule>& rule) : weak_(rule) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
        auto rule = weak_.lock();
        assert(rule);
        return rule->parse(s, l, sv);
    }

private:
    std::weak_ptr<Rule> weak_;
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

class SemanticAction
{
public:
    operator bool() const {
        return (bool)fn_;
    }

    Any operator()(const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) const {
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
 * Definition
 */
class Definition
{
public:
    Definition() : rule_(std::make_shared<NonTerminal>(this)) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , rule_(rhs.rule_)
    {
        non_terminal().outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , rule_(std::move(rhs.rule_))
    {
        non_terminal().outer_ = this;
    }

    Definition(const std::shared_ptr<Rule>& rule)
        : rule_(std::make_shared<NonTerminal>(this))
    {
        set_rule(rule);
    }

    operator std::shared_ptr<Rule>() {
        return std::make_shared<WeakHolder>(rule_);
    }

    Definition& operator<=(const std::shared_ptr<Rule>& rule) {
        set_rule(rule);
        return *this;
    }

    template <typename T>
    bool parse(const char* s, size_t l, T& val) const {
        SemanticValues sv;

        auto m = rule_->parse(s, l, sv);
        auto ret = m.ret && m.len == l;

        if (ret && !sv.values.empty() && !sv.values.front().is_undefined()) {
            val = sv.values[0].get<T>();
        }

        return ret;
    }

    template <typename T>
    bool parse(const char* s, T& val) const {
        return parse(s, strlen(s), val);
    }

    bool parse(const char* s, size_t l) const {
        SemanticValues sv;
        auto m = rule_->parse(s, l, sv);
        return m.ret && m.len == l;
    }

    bool parse(const char* s) const {
        return parse(s, strlen(s));
    }

    template <typename F>
    void operator,(F fn) {
        action = fn;
    }

    std::string    name;
    SemanticAction action;

private:
    friend class DefinitionReference;

    class NonTerminal : public Rule
    {
    public:
        NonTerminal(Definition* outer) : outer_(outer) {};

        Match parse(const char* s, size_t l, SemanticValues& sv) const {
            if (!rule_) {
                throw std::logic_error("Uninitialized definition rule was used...");
            }

            SemanticValues chldsv;

            auto m = rule_->parse(s, l, chldsv);
            if (m.ret) {
                 sv.names.push_back(outer_->name);
                 auto val = reduce(s, m.len, chldsv, outer_->action);
                 sv.values.push_back(val);
            }

            return m;
        }

    private:
        friend class Definition;

        template<typename Action>
        Any reduce(const char* s, size_t l, const SemanticValues& sv, Action action) const {
            if (action) {
                return action(s, l, sv.values, sv.names);
            } else if (sv.values.empty()) {
                return Any();
            } else {
                return sv.values.front();
            }
        }

        std::shared_ptr<Rule> rule_;
        Definition*           outer_;
    };

    Definition& operator=(const Definition& rhs);
    Definition& operator=(Definition&& rhs);

    NonTerminal& non_terminal() {
        return *dynamic_cast<NonTerminal*>(rule_.get());
    }

    void set_rule(const std::shared_ptr<Rule>& rule) {
        non_terminal().rule_ = rule;
    }

    std::shared_ptr<Rule> rule_;
};

class DefinitionReference : public Rule
{
public:
    DefinitionReference(
        const std::map<std::string, Definition>& grammar, const std::string& name)
        : grammar_(grammar)
        , name_(name) {}

    Match parse(const char* s, size_t l, SemanticValues& sv) const {
       auto rule = grammar_.at(name_).rule_;
       return rule->parse(s, l, sv);
    }

private:
    const std::map<std::string, Definition>& grammar_;
    std::string name_;
};

/*
 * Factories
 */
template <typename... Args>
std::shared_ptr<Rule> seq(Args&& ...args) {
    return std::make_shared<Sequence>(static_cast<std::shared_ptr<Rule>>(args)...);
}

inline std::shared_ptr<Rule> seq_v(const std::vector<std::shared_ptr<Rule>>& rules) {
    return std::make_shared<Sequence>(rules);
}

inline std::shared_ptr<Rule> seq_v(std::vector<std::shared_ptr<Rule>>&& rules) {
    return std::make_shared<Sequence>(std::move(rules));
}

template <typename... Args>
std::shared_ptr<Rule> cho(Args&& ...args) {
    return std::make_shared<PrioritizedChoice>(static_cast<std::shared_ptr<Rule>>(args)...);
}

inline std::shared_ptr<Rule> cho_v(const std::vector<std::shared_ptr<Rule>>& rules) {
    return std::make_shared<PrioritizedChoice>(rules);
}

inline std::shared_ptr<Rule> cho_v(std::vector<std::shared_ptr<Rule>>&& rules) {
    return std::make_shared<PrioritizedChoice>(std::move(rules));
}

inline std::shared_ptr<Rule> zom(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<ZeroOrMore>(rule);
}

inline std::shared_ptr<Rule> oom(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<OneOrMore>(rule);
}

inline std::shared_ptr<Rule> opt(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Option>(rule);
}

inline std::shared_ptr<Rule> apd(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<AndPredicate>(rule);
}

inline std::shared_ptr<Rule> npd(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<NotPredicate>(rule);
}

inline std::shared_ptr<Rule> lit(const char* lit) {
    return std::make_shared<LiteralString>(lit);
}

inline std::shared_ptr<Rule> cls(const char* chars) {
    return std::make_shared<CharacterClass>(chars);
}

inline std::shared_ptr<Rule> chr(char c) {
    return std::make_shared<Character>(c);
}

inline std::shared_ptr<Rule> any() {
    return std::make_shared<AnyCharacter>();
}

inline std::shared_ptr<Rule> grp(const std::shared_ptr<Rule>& rule) {
    return std::make_shared<Grouping>(rule);
}

inline std::shared_ptr<Rule> grp(const std::shared_ptr<Rule>& rule, std::function<void (const char* s, size_t l)> match) {
    return std::make_shared<Grouping>(rule, match);
}

inline std::shared_ptr<Rule> ref(const std::map<std::string, Definition>& grammar, const std::string& name) {
    return std::make_shared<DefinitionReference>(grammar, name);
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
    std::set<std::string> refs;

    peg["Definition"].action = [&](const std::vector<Any>& v) {
        const auto& name = v[0].get<std::string>();
        (*grammar)[name] <= v[2].get<std::shared_ptr<Rule>>();
        (*grammar)[name].name = name;

        if (start.empty()) {
            start = name;
        }
    };

    peg["Expression"].action = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Rule>> rules;
        for (auto i = 0u; i < v.size(); i++) {
            if (!(i % 2)) {
                rules.push_back(v[i].get<std::shared_ptr<Rule>>());
            }
        }
        return cho_v(rules);
    };

    peg["Sequence"].action = [&](const std::vector<Any>& v) {
        std::vector<std::shared_ptr<Rule>> rules;
        for (const auto& x: v) {
            rules.push_back(x.get<std::shared_ptr<Rule>>());
        }
        return seq_v(rules);
    };

    peg["Prefix"].action = [&](const std::vector<Any>& v, const std::vector<std::string>& n) {
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

    peg["Suffix"].action = [&](const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
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

    peg["Primary"].action = [&](const char* s, size_t l, const std::vector<Any>& v, const std::vector<std::string>& n) {
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

    peg["IdentCont"].action = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["Literal"].action = [](const std::vector<Any>& v) {
        return lit(v[0].get<std::string>().c_str());
    };
    peg["SQCont"].action = [](const char*s, size_t l) {
        return std::string(s, l);
    };
    peg["DQCont"].action = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["Class"].action = [](const std::vector<Any>& v) {
        return cls(v[0].get<std::string>().c_str());
    };
    peg["ClassCont"].action = [](const char*s, size_t l) {
        return std::string(s, l);
    };

    peg["DOT"].action = []() {
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

    SemanticAction& operator[](const char* s) {
        return (*grammar_)[s].action;
    }

private:
    std::shared_ptr<Grammar> grammar_;
    std::string              start_;
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
