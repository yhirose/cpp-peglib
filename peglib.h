//
//  peglib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#define CPPPEGLIB_VERSION "1.24.0"
#define CPPPEGLIB_VERSION_NUM "0x011800"

/*
 * Configuration
 */

#ifndef CPPPEGLIB_HEURISTIC_ERROR_TOKEN_MAX_CHAR_COUNT
#define CPPPEGLIB_HEURISTIC_ERROR_TOKEN_MAX_CHAR_COUNT 32
#endif

#if defined(_MSC_VER)
#define CPPPEGLIB_NOINLINE __declspec(noinline)
#else
#define CPPPEGLIB_NOINLINE __attribute__((noinline))
#endif

#include <algorithm>
#include <any>
#include <bitset>
#include <cassert>
#include <cctype>
#if __has_include(<charconv>)
#include <charconv>
#endif
#include <cstdint>
#ifdef CPPPEGLIB_TEST
#include <cstdlib>
#endif
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if !defined(__cplusplus) || __cplusplus < 201703L
#error "Requires complete C++17 support"
#endif

namespace peg {

struct GrammarBlob;

/*-----------------------------------------------------------------------------
 *  scope_exit
 *---------------------------------------------------------------------------*/

// This is based on
// "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189".

template <typename EF> struct scope_exit {
  explicit scope_exit(EF &&f)
      : exit_function(std::move(f)), execute_on_destruction{true} {}

  scope_exit(scope_exit &&rhs)
      : exit_function(std::move(rhs.exit_function)),
        execute_on_destruction{rhs.execute_on_destruction} {
    rhs.release();
  }

  ~scope_exit() {
    if (execute_on_destruction) { this->exit_function(); }
  }

  void release() { this->execute_on_destruction = false; }

private:
  scope_exit(const scope_exit &) = delete;
  void operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&) = delete;

  EF exit_function;
  bool execute_on_destruction;
};

/*-----------------------------------------------------------------------------
 *  UTF8 functions
 *---------------------------------------------------------------------------*/

// Only a well-formed sequence is a code point: not an overlong form, a
// surrogate, a code point past U+10FFFF, a sequence cut short or a stray
// byte.
inline bool decode_codepoint(const char *s8, size_t l, size_t &bytes,
                             char32_t &cp) {
  if (!l) { return false; }
  auto b = static_cast<uint8_t>(s8[0]);
  if (b < 0x80) {
    bytes = 1;
    cp = b;
    return true;
  }
  size_t len = 0;
  char32_t min = 0;
  char32_t value = 0;
  if ((b & 0xE0) == 0xC0) {
    len = 2;
    value = b & 0x1F;
    min = 0x80;
  } else if ((b & 0xF0) == 0xE0) {
    len = 3;
    value = b & 0x0F;
    min = 0x800;
  } else if ((b & 0xF8) == 0xF0) {
    len = 4;
    value = b & 0x07;
    min = 0x10000;
  }
  if (!len || len > l) { return false; }
  for (size_t i = 1; i < len; i++) {
    auto c = static_cast<uint8_t>(s8[i]);
    if ((c & 0xC0) != 0x80) { return false; }
    value = (value << 6) | (c & 0x3F);
  }
  if (value < min || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
    return false;
  }
  bytes = len;
  cp = value;
  return true;
}

// 0 where no well-formed code point starts.
inline size_t codepoint_length(const char *s8, size_t l) {
  size_t bytes = 0;
  char32_t cp;
  decode_codepoint(s8, l, bytes, cp);
  return bytes;
}

// Text shown to a person steps over what is not well-formed a byte at a
// time.
inline size_t codepoint_or_byte_length(const char *s8, size_t l) {
  auto len = codepoint_length(s8, l);
  return len || !l ? len : 1;
}

inline size_t codepoint_count(const char *s8, size_t l) {
  size_t count = 0;
  for (size_t i = 0; i < l; i += codepoint_or_byte_length(s8 + i, l - i)) {
    count++;
  }
  return count;
}

inline size_t encode_codepoint(char32_t cp, char *buff) {
  if (cp < 0x0080) {
    buff[0] = static_cast<char>(cp & 0x7F);
    return 1;
  } else if (cp < 0x0800) {
    buff[0] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
    buff[1] = static_cast<char>(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp < 0xD800) {
    buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
    buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  } else if (cp < 0xE000) {
    // D800 - DFFF is invalid...
    return 0;
  } else if (cp < 0x10000) {
    buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
    buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  } else if (cp < 0x110000) {
    buff[0] = static_cast<char>(0xF0 | ((cp >> 18) & 0x7));
    buff[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    buff[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
  }
  return 0;
}

inline std::string encode_codepoint(char32_t cp) {
  char buff[4];
  auto l = encode_codepoint(cp, buff);
  return std::string(buff, l);
}

inline size_t decode_codepoint(const char *s8, size_t l, char32_t &cp) {
  size_t bytes;
  if (decode_codepoint(s8, l, bytes, cp)) { return bytes; }
  return 0;
}

inline char32_t decode_codepoint(const char *s8, size_t l) {
  char32_t cp = 0;
  decode_codepoint(s8, l, cp);
  return cp;
}

inline std::u32string decode(const char *s8, size_t l) {
  std::u32string out;
  for (size_t i = 0; i < l; i += codepoint_or_byte_length(s8 + i, l - i)) {
    out += decode_codepoint(s8 + i, l - i);
  }
  return out;
}

template <typename T> const char *u8(const T *s) {
  return reinterpret_cast<const char *>(s);
}

/*-----------------------------------------------------------------------------
 *  escape_characters
 *---------------------------------------------------------------------------*/

inline std::string escape_characters(const char *s, size_t n) {
  std::string str;
  for (size_t i = 0; i < n; i++) {
    auto c = s[i];
    switch (c) {
    case '\f': str += "\\f"; break;
    case '\n': str += "\\n"; break;
    case '\r': str += "\\r"; break;
    case '\t': str += "\\t"; break;
    case '\v': str += "\\v"; break;
    default: str += c; break;
    }
  }
  return str;
}

inline std::string escape_characters(std::string_view sv) {
  return escape_characters(sv.data(), sv.size());
}

/*-----------------------------------------------------------------------------
 *  resolve_escape_sequence
 *---------------------------------------------------------------------------*/

inline bool is_hex(char c, int &v) {
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

inline bool is_digit(char c, int &v) {
  if ('0' <= c && c <= '9') {
    v = c - '0';
    return true;
  }
  return false;
}

inline bool is_octal(char c, int &v) {
  if ('0' <= c && c <= '7') {
    v = c - '0';
    return true;
  }
  return false;
}

inline std::pair<int, size_t> parse_hex_number(const char *s, size_t n,
                                               size_t i, size_t max_digits) {
  int ret = 0;
  int val;
  const auto end = i + std::min(max_digits, n - i);
  while (i < end && is_hex(s[i], val)) {
    ret = static_cast<int>(ret * 16 + val);
    i++;
  }
  return std::pair(ret, i);
}

inline std::pair<int, size_t> parse_octal_number(const char *s, size_t n,
                                                 size_t i, size_t max_digits) {
  int ret = 0;
  int val;
  const auto end = i + std::min(max_digits, n - i);
  while (i < end && is_octal(s[i], val)) {
    ret = static_cast<int>(ret * 8 + val);
    i++;
  }
  return std::pair(ret, i);
}

inline std::string resolve_escape_sequence(const char *s, size_t n) {
  std::string r;
  r.reserve(n);

  size_t i = 0;
  while (i < n) {
    auto ch = s[i];
    if (ch == '\\') {
      i++;
      if (i == n) {
        r += '\\';
        break;
      }

      // An escape takes only as many digits as the grammar's Char rule does,
      // so a digit after it is a character of its own.
      switch (s[i]) {
      case 'f':
        r += '\f';
        i++;
        break;
      case 'n':
        r += '\n';
        i++;
        break;
      case 'r':
        r += '\r';
        i++;
        break;
      case 't':
        r += '\t';
        i++;
        break;
      case 'v':
        r += '\v';
        i++;
        break;
      case '\'':
        r += '\'';
        i++;
        break;
      case '"':
        r += '"';
        i++;
        break;
      case '[':
        r += '[';
        i++;
        break;
      case ']':
        r += ']';
        i++;
        break;
      case '^':
        r += '^';
        i++;
        break;
      case '-':
        r += '-';
        i++;
        break;
      case '\\':
        r += '\\';
        i++;
        break;
      case 'x': {
        char32_t cp;
        std::tie(cp, i) = parse_hex_number(s, n, i + 1, 2);
        r += encode_codepoint(cp);
        break;
      }
      case 'u': {
        // Six digits from 0 or 10 (up to U+10FFFF), else four or five.
        const auto six = i + 2 < n && (s[i + 1] == '0' ||
                                       (s[i + 1] == '1' && s[i + 2] == '0'));
        char32_t cp;
        std::tie(cp, i) = parse_hex_number(s, n, i + 1, six ? 6 : 5);
        r += encode_codepoint(cp);
        break;
      }
      default: {
        // Three digits up to \377, else two.
        const auto three = '0' <= s[i] && s[i] <= '3';
        char32_t cp;
        std::tie(cp, i) = parse_octal_number(s, n, i, three ? 3 : 2);
        r += encode_codepoint(cp);
        break;
      }
      }
    } else {
      r += ch;
      i++;
    }
  }
  return r;
}

/*
 * Predefined character classes (ASCII semantics)
 */
inline const std::vector<std::pair<char32_t, char32_t>> *
predefined_character_class(std::string_view name) {
  static const std::map<std::string_view,
                        std::vector<std::pair<char32_t, char32_t>>>
      table = {
          {"alnum", {{'0', '9'}, {'A', 'Z'}, {'a', 'z'}}},
          {"alpha", {{'A', 'Z'}, {'a', 'z'}}},
          {"ascii", {{0x00, 0x7F}}},
          {"blank", {{'\t', '\t'}, {' ', ' '}}},
          {"cntrl", {{0x00, 0x1F}, {0x7F, 0x7F}}},
          {"digit", {{'0', '9'}}},
          {"graph", {{0x21, 0x7E}}},
          {"lower", {{'a', 'z'}}},
          {"print", {{0x20, 0x7E}}},
          {"punct", {{0x21, 0x2F}, {0x3A, 0x40}, {0x5B, 0x60}, {0x7B, 0x7E}}},
          {"space", {{'\t', '\r'}, {' ', ' '}}},
          {"upper", {{'A', 'Z'}}},
          {"word", {{'0', '9'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}}},
          {"xdigit", {{'0', '9'}, {'A', 'F'}, {'a', 'f'}}},
      };
  auto it = table.find(name);
  return it != table.end() ? &it->second : nullptr;
}

inline std::vector<std::pair<char32_t, char32_t>>
complement_character_ranges(std::vector<std::pair<char32_t, char32_t>> ranges) {
  std::sort(ranges.begin(), ranges.end());
  std::vector<std::pair<char32_t, char32_t>> r;
  char32_t next = 0;
  for (const auto &[lo, hi] : ranges) {
    if (lo > next) { r.emplace_back(next, lo - 1); }
    next = std::max<char32_t>(next, hi + 1);
  }
  if (next <= 0x10FFFF) { r.emplace_back(next, 0x10FFFF); }
  return r;
}

// Only an ASCII letter has another case.
inline char32_t other_case(char32_t cp) {
  if ('a' <= cp && cp <= 'z') { return cp - 'a' + 'A'; }
  if ('A' <= cp && cp <= 'Z') { return cp - 'A' + 'a'; }
  return cp;
}

inline std::vector<std::pair<char32_t, char32_t>>
add_other_case(const std::vector<std::pair<char32_t, char32_t>> &ranges) {
  auto r = ranges;
  for (const auto &[lo, hi] : ranges) {
    for (auto cp = lo; cp <= std::min<char32_t>(hi, 'z'); cp++) {
      if (auto other = other_case(cp); other != cp) {
        r.emplace_back(other, other);
      }
    }
  }
  return r;
}

inline char to_lower(char c) {
  return 'A' <= c && c <= 'Z' ? static_cast<char>(other_case(c)) : c;
}

inline std::string to_lower(std::string s) {
  for (auto &c : s) {
    c = to_lower(c);
  }
  return s;
}

/*-----------------------------------------------------------------------------
 *  token_to_number_ - This function should be removed eventually
 *---------------------------------------------------------------------------*/

template <typename T> T token_to_number_(std::string_view sv) {
  T n = 0;
#if __has_include(<charconv>)
  if constexpr (!std::is_floating_point<T>::value) {
    std::from_chars(sv.data(), sv.data() + sv.size(), n);
#else
  if constexpr (false) {
#endif
  } else {
    auto s = std::string(sv);
    std::istringstream ss(s);
    ss >> n;
  }
  return n;
}

/*-----------------------------------------------------------------------------
 *  Trie
 *---------------------------------------------------------------------------*/

class Trie {
public:
  Trie(const std::vector<std::string> &items, bool ignore_case)
      : ignore_case_(ignore_case), items_count_(items.size()) {
    size_t id = 0;
    for (const auto &item : items) {
      const auto &s = ignore_case ? to_lower(item) : item;
      if (item.size() > max_len_) { max_len_ = item.size(); }
      for (size_t len = 1; len <= item.size(); len++) {
        auto last = len == item.size();
        std::string_view sv(s.data(), len);
        auto it = dic_.find(sv);
        if (it == dic_.end()) {
          it = dic_.emplace(sv, Info{true, false, 0}).first;
        }
        auto &info = it->second;
        // A word listed twice keeps its first id, as in a choice.
        if (!last) {
          info.done = false;
        } else if (!info.match) {
          info.match = true;
          info.id = id;
        }
      }
      id++;
    }
  }

  size_t match(const char *text, size_t text_len, size_t &id) const {
    auto limit = std::min(text_len, max_len_);
    std::string lower_text;
    if (ignore_case_) {
      lower_text = to_lower(std::string(text, limit));
      text = lower_text.data();
    }

    size_t match_len = 0;
    auto done = false;
    size_t len = 1;
    while (!done && len <= limit) {
      std::string_view sv(text, len);
      auto it = dic_.find(sv);
      if (it == dic_.end()) {
        done = true;
      } else {
        if (it->second.match) {
          match_len = len;
          id = it->second.id;
        }
        if (it->second.done) { done = true; }
      }
      len += 1;
    }
    return match_len;
  }

  size_t size() const { return dic_.size(); }
  size_t items_count() const { return items_count_; }

  friend struct ComputeFirstSet;
  friend struct GrammarBlob;

private:
  struct Info {
    bool done;
    bool match;
    size_t id;
  };

  // TODO: Use unordered_map when heterogeneous lookup is supported in C++20
  // std::unordered_map<std::string, Info> dic_;
  std::map<std::string, Info, std::less<>> dic_;

  bool ignore_case_;
  size_t items_count_;
  size_t max_len_ = 0;
};

/*-----------------------------------------------------------------------------
 *  PEG
 *---------------------------------------------------------------------------*/

/*
 * Line information utility function
 */
inline std::pair<size_t, size_t> line_info(const char *start, const char *cur) {
  auto p = start;
  auto col_ptr = p;
  auto no = 1;

  while (p < cur) {
    if (*p == '\n') {
      no++;
      col_ptr = p + 1;
    }
    p++;
  }

  auto col = codepoint_count(col_ptr, p - col_ptr) + 1;

  return std::pair(no, col);
}

/*
 * String tag
 */
inline constexpr unsigned int str2tag_core(const char *s, size_t l,
                                           unsigned int h) {
  return (l == 0) ? h
                  : str2tag_core(s + 1, l - 1,
                                 (h * 33) ^ static_cast<unsigned char>(*s));
}

inline constexpr unsigned int str2tag(std::string_view sv) {
  return str2tag_core(sv.data(), sv.size(), 0);
}

namespace udl {

inline constexpr unsigned int operator""_(const char *s, size_t l) {
  return str2tag_core(s, l, 0);
}

} // namespace udl

/*
 * Semantic values
 */
class Context;

struct SemanticValues : protected std::vector<std::any> {
  SemanticValues() = default;
  SemanticValues(Context *c) : c_(c) {}

  // Input text
  const char *path = nullptr;
  const char *ss = nullptr;

  // Matched string
  std::string_view sv() const { return sv_; }

  // Definition name
  const std::string &name() const { return *name_; }

  std::vector<unsigned int> tags;

  // Line number and column at which the matched string is
  std::pair<size_t, size_t> line_info() const;

  // Choice count
  size_t choice_count() const { return choice_count_; }

  // Choice number (0 based index)
  size_t choice() const { return choice_; }

  // Tokens
  std::vector<std::string_view> tokens;

  std::string_view token(size_t id = 0) const {
    if (tokens.empty()) { return sv_; }
    assert(id < tokens.size());
    return tokens[id];
  }

  // Token conversion
  std::string token_to_string(size_t id = 0) const {
    return std::string(token(id));
  }

  template <typename T> T token_to_number() const {
    return token_to_number_<T>(token());
  }

  // Transform the semantic value vector to another vector
  template <typename T>
  std::vector<T> transform(size_t beg = 0,
                           size_t end = static_cast<size_t>(-1)) const {
    std::vector<T> r;
    end = (std::min)(end, size());
    if (end > beg) { r.reserve(end - beg); }
    for (size_t i = beg; i < end; i++) {
      r.emplace_back(std::any_cast<T>((*this)[i]));
    }
    return r;
  }

  using std::vector<std::any>::iterator;
  using std::vector<std::any>::const_iterator;
  using std::vector<std::any>::size;
  using std::vector<std::any>::empty;
  using std::vector<std::any>::assign;
  using std::vector<std::any>::begin;
  using std::vector<std::any>::end;
  using std::vector<std::any>::rbegin;
  using std::vector<std::any>::rend;
  using std::vector<std::any>::operator[];
  using std::vector<std::any>::at;
  using std::vector<std::any>::resize;
  using std::vector<std::any>::front;
  using std::vector<std::any>::back;
  using std::vector<std::any>::push_back;
  using std::vector<std::any>::pop_back;
  using std::vector<std::any>::insert;
  using std::vector<std::any>::erase;
  using std::vector<std::any>::clear;
  using std::vector<std::any>::swap;
  using std::vector<std::any>::emplace;
  using std::vector<std::any>::emplace_back;

private:
  friend class Context;
  friend class Dictionary;
  friend class Sequence;
  friend class PrioritizedChoice;
  friend class Repetition;
  friend class Holder;
  friend class PrecedenceClimbing;
  friend class TokenBoundary;

  static const std::string &empty_name() {
    static const std::string name;
    return name;
  }

  Context *c_ = nullptr;
  std::string_view sv_;
  size_t choice_count_ = 0;
  size_t choice_ = 0;
  // Set on the scope a precedence parses its operator into: its tokens are
  // those of the rule matches delivered here, not its token boundaries.
  bool holds_rule_tokens_ = false;
  // Cleared where nothing reads the values of the rule matches delivered
  // here, which are then not delivered.
  bool holds_rule_values_ = true;
  // Cleared on the one scope that nothing reads at all (see
  // Context::unread_scope): it holds no tokens either.
  bool is_read_ = true;
  // Points at the matched rule's name (owned by the Definition, which
  // outlives the parse); assigning a pointer beats copying a string on
  // every successful rule match.
  const std::string *name_ = &empty_name();
};

/*
 * Semantic action
 */
template <typename F, typename... Args> std::any call(F fn, Args &&...args) {
  using R = decltype(fn(std::forward<Args>(args)...));
  if constexpr (std::is_void<R>::value) {
    fn(std::forward<Args>(args)...);
    return std::any();
  } else if constexpr (std::is_same<typename std::remove_cv<R>::type,
                                    std::any>::value) {
    return fn(std::forward<Args>(args)...);
  } else {
    return std::any(fn(std::forward<Args>(args)...));
  }
}

template <typename T>
struct argument_count : argument_count<decltype(&T::operator())> {};
template <typename R, typename... Args>
struct argument_count<R (*)(Args...)>
    : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct argument_count<R (C::*)(Args...)>
    : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct argument_count<R (C::*)(Args...) const>
    : std::integral_constant<unsigned, sizeof...(Args)> {};

class Action {
public:
  Action() = default;
  Action(const Action &) = default;
  Action(Action &&) = default;
  Action &operator=(const Action &) = default;
  Action &operator=(Action &&) = default;
  template <typename F> Action(F fn) : fn_(make_adaptor(fn)) {}
  template <typename F> Action &operator=(F fn) {
    fn_ = make_adaptor(fn);
    ast_node_type_ = nullptr;
    return *this;
  }

  operator bool() const { return bool(fn_); }

  // Declares this action to be an AST action: it builds a node, held as
  // `Node`, from the values alone (a token rule's node from its token, not
  // reading the values), so a parse need not run it where nothing reads the
  // node. Assigning another function clears the declaration.
  template <typename Node> void declare_ast_action(bool /*collapse*/) {
    ast_node_type_ = &typeid(Node);
  }
  const std::type_info *ast_node_type() const { return ast_node_type_; }

  std::any operator()(SemanticValues &vs, std::any &dt,
                      const std::any &predicate_data) const {
    return fn_(vs, dt, predicate_data);
  }

private:
  using Fty = std::function<std::any(SemanticValues &vs, std::any &dt,
                                     const std::any &predicate_data)>;

  template <typename F> Fty make_adaptor(F fn) {
    if constexpr (argument_count<F>::value == 1) {
      return [fn](auto &vs, auto & /*dt*/, const auto & /*predicate_data*/) {
        return call(fn, vs);
      };
    } else if constexpr (argument_count<F>::value == 2) {
      return [fn](auto &vs, auto &dt, const auto & /*predicate_data*/) {
        return call(fn, vs, dt);
      };
    } else {
      return [fn](auto &vs, auto &dt, const auto &predicate_data) {
        return call(fn, vs, dt, predicate_data);
      };
    }
  }

  Fty fn_;
  const std::type_info *ast_node_type_ = nullptr;
};

class Predicate {
public:
  Predicate() = default;
  Predicate(const Predicate &) = default;
  Predicate(Predicate &&) = default;
  Predicate &operator=(const Predicate &) = default;
  Predicate &operator=(Predicate &&) = default;
  template <typename F> Predicate(F fn) : fn_(make_adaptor(fn)) {}
  template <typename F> Predicate &operator=(F fn) {
    fn_ = make_adaptor(fn);
    return *this;
  }

  operator bool() const { return bool(fn_); }

  bool operator()(const SemanticValues &vs, const std::any &dt,
                  std::string &msg, std::any &predicate_data) const {
    return fn_(vs, dt, msg, predicate_data);
  }

private:
  using Fty = std::function<bool(const SemanticValues &vs, const std::any &dt,
                                 std::string &msg, std::any &predicate_data)>;

  template <typename F> Fty make_adaptor(F fn) {
    if constexpr (argument_count<F>::value == 3) {
      return [fn](const auto &vs, const auto &dt, auto &msg,
                  auto & /*predicate_data*/) { return fn(vs, dt, msg); };
    } else {
      return [fn](const auto &vs, const auto &dt, auto &msg,
                  auto &predicate_data) {
        return fn(vs, dt, msg, predicate_data);
      };
    }
  }

  Fty fn_;
};

/*
 * Parse result helper
 */
// The length of a parse that did not match.
inline constexpr size_t kFail = static_cast<size_t>(-1);

inline bool success(size_t len) { return len != kFail; }

inline bool fail(size_t len) { return len == kFail; }

/*
 * Log
 */
using Log = std::function<void(size_t line, size_t col, const std::string &msg,
                               const std::string &rule)>;

/*
 * ErrorReport - structured error information passed to an ErrorReporter.
 * Unlike Log, nothing is flattened into a display string, so applications
 * can map errors to their own error types, localize messages, or feed
 * diagnostics to IDEs.
 */
struct ErrorReport {
  size_t line = 0;              // 1-based
  size_t col = 1;               // 1-based
  size_t position = 0;          // byte offset in the input
  std::string unexpected_token; // heuristic token at the error position
  std::vector<std::string> expected_literals;
  std::vector<std::string> expected_rules; // rules starting with '_' excluded
  std::string message; // custom error_message if any (placeholders resolved)
  std::string label;   // rule name or recovery label the error belongs to
};

using ErrorReporter = std::function<void(const ErrorReport &report)>;

/*
 * ErrorInfo
 */
class Definition;
class Holder;
class Reference;
class PrioritizedChoice;
struct FirstSet;

// What fails where an expression cannot start: a failure of its own, of
// `literal` if it is a literal, or what fails where `rule`, which it refers
// to, cannot start.
struct StartFailure {
  const char *literal = nullptr;
  const Definition *rule = nullptr;
  bool listed = true;
};

// Thrown when a parse nests more rule matches than its start rule's
// max_depth allows; that parse catches it (see Definition::parse_core).
struct NestingTooDeep {
  const char *pos;
  const Definition *rule;
};

// Mixes the bits of a key for open addressing. In 64 bits so that `h >> 32`
// stays well-defined where size_t is 32-bit (wasm32).
inline size_t mix_index(size_t key) {
  auto h = static_cast<uint64_t>(key) * 0x9E3779B97F4A7C15ull;
  return static_cast<size_t>(h ^ (h >> 32));
}

// Drops the repeats in `v` from `from` on, keeping the first of each in
// order, and returns how many went. The items kept so far are found by `hash`,
// so that this takes time linear in the number of items.
template <typename T, typename Hash>
size_t drop_repeats(std::vector<T> &v, size_t from, Hash hash) {
  if (v.size() - from < 2) { return 0; }
  size_t slots = 1;
  while (slots < 2 * (v.size() - from)) {
    slots *= 2;
  }
  std::vector<size_t> kept_at(slots); // 1 + where an item is kept, or 0
  auto kept = from;
  for (auto i = from; i < v.size(); i++) {
    auto j = hash(v[i]) & (slots - 1);
    while (kept_at[j] && !(v[kept_at[j] - 1] == v[i])) {
      j = (j + 1) & (slots - 1);
    }
    if (!kept_at[j]) {
      v[kept] = v[i];
      kept_at[j] = ++kept;
    }
  }
  auto dropped = v.size() - kept;
  v.resize(kept);
  return dropped;
}

// A call of a rule: the body it parses (a macro has one per instance), where
// it starts, and whether whitespace is skipped there. A rule may match
// differently where none is, so whatever stands for a call, a memo, a seed
// or a rule being parsed, is kept by all three.
struct RuleCall {
  const Holder *rule = nullptr;
  const char *pos = nullptr;
  bool skips_no_whitespace = false;

  bool operator==(const RuleCall &c) const {
    return pos == c.pos && rule == c.rule &&
           skips_no_whitespace == c.skips_no_whitespace;
  }
  bool operator<(const RuleCall &c) const {
    return std::tie(pos, rule, skips_no_whitespace) <
           std::tie(c.pos, c.rule, c.skips_no_whitespace);
  }
};

struct ErrorInfo {
  const char *error_pos = nullptr;
  std::vector<std::pair<const char *, const Definition *>> expected_tokens;
  const char *message_pos = nullptr;
  std::string message;
  std::string label;
  const char *last_output_pos = nullptr;

  void clear() {
    error_pos = nullptr;
    clear_expected_tokens();
    clear_message();
  }

  void clear_message() {
    message_pos = nullptr;
    message.clear();
    label.clear();
  }

  void clear_expected_tokens() {
    next_generation();
    expected_tokens.clear();
    literal_copies_.clear();
  }

  // False when a failure has gone past `pos`. Otherwise the error is at
  // `pos` now, and what was expected before it is dropped.
  bool advance_to(const char *pos) {
    if (pos < error_pos) { return false; }
    if (error_pos < pos) {
      error_pos = pos;
      clear_expected_tokens();
    }
    return true;
  }

  // What a failure expected is noted as an event, and turned into
  // expected_tokens only when a parse returns or a message is built: nearly
  // all of it is noted at a position the parse then moves past, and is never
  // read.
  //
  // A literal points at text the grammar owns, unless `copy_literal` is set:
  // a back reference's literal is captured text, which is dropped when the
  // parse backtracks past its capture, and when the parse ends.
  void add(StartFailure failure, const Definition *rule,
           const Definition *token_rule, bool copy_literal) {
    if (copy_literal) { failure.literal = copy_of(failure.literal); }
    note({Kind::failure, failure, rule, token_rule, nullptr, 0, 0, 0});
  }

  // The alternatives `begin` to `end` of `choice`, in `rule`, were skipped by
  // their first sets.
  void add_skipped(const PrioritizedChoice *choice, size_t begin, size_t end,
                   const Definition *rule, const Definition *token_rule) {
    note({Kind::skipped, {}, rule, token_rule, choice, begin, end, 0});
  }

  // What a parse whose match is memoized notes is kept with the memo, so
  // that reusing the memo notes it again, as parsing the rule again would.
  // It is kept from where the parse began, as a record of this generation:
  // the events at one error position. The parse around it names what it
  // notes by `token_rule`, if there is one.
  void open_frame(const Definition *token_rule) {
    frames_.push_back({events_.size(), generation_, token_rule});
    boundary_ = events_.size();
  }

  // What the innermost memoized parse noted is kept for its memo, `key`,
  // in place of what a parse of a memo made before with the same key kept.
  void close_frame(const RuleCall &key) {
    auto span = span_of(frames_.back());
    frames_.pop_back();
    boundary_ = events_.size();
    if (span.begin < span.end) {
      records_.push_back(span);
      keep_record(key, records_.size() - 1);
    } else if (!record_slots_.empty()) {
      auto &slot = record_slots_[slot_of(key)];
      if (slot.generation == generation_) { slot.record = no_record; }
    }
  }

  // Notes again what was kept for the memo `key`, under `token_rule` if
  // there is one.
  void replay(const RuleCall &key, const Definition *token_rule) {
    if (record_slots_.empty()) { return; }
    const auto &slot = record_slots_[slot_of(key)];
    if (slot.generation == generation_ && slot.record != no_record) {
      note({Kind::replay, {}, nullptr, token_rule, nullptr, 0, 0, slot.record});
    }
  }

  void resolve_expected_tokens();

  // A message stands for the error at error_pos: once a failure goes past
  // that, the message is not read, and resolving the error drops it.
  void set_message(const char *pos, std::string msg,
                   const std::string &lbl = "") {
    message_pos = pos;
    message = std::move(msg);
    label = lbl;
    message_error_pos_ = error_pos;
  }
  bool message_lies_at_or_after(const char *pos) const {
    return has_message() && pos <= message_pos;
  }

  // Whether anything has been noted, or the error has moved on or been
  // resolved, since mark() was taken.
  struct Mark {
    size_t generation;
    size_t noted;
  };
  Mark mark() const { return {generation_, events_.size() + removed_}; }
  bool noted_since(const Mark &m) const {
    return generation_ != m.generation || events_.size() + removed_ != m.noted;
  }

  // A parse went past its max_depth at `pos`, where the rule match or the
  // returned AST node that `label` names starts. It is reported even at or
  // before a position a recovered error already was.
  void set_nesting_too_deep(const char *pos, const std::string &label,
                            size_t max_depth) {
    set_message(pos,
                "exceeded the maximum nesting depth of " +
                    std::to_string(max_depth),
                label);
    last_output_pos = nullptr;
  }

  void output_log(const Log &log, const char *s, size_t n) {
    output_log(log, nullptr, s, n);
  }
  // An error that a recovery reports is labeled with `recovery_label`.
  void output_log(const Log &log, const ErrorReporter &reporter, const char *s,
                  size_t n, const std::string &recovery_label = "");

private:
  // What failed in `rule`: `failure`, or the alternatives `begin` to `end`
  // that `choice` skipped. Or `record`, noted again.
  enum class Kind : uint8_t { failure, skipped, replay };
  struct ExpectedEvent {
    Kind kind;
    StartFailure failure;
    const Definition *rule;
    const Definition *token_rule; // the outermost token rule around it
    const PrioritizedChoice *choice;
    size_t begin;
    size_t end;
    size_t record;

    bool operator==(const ExpectedEvent &e) const {
      return kind == e.kind && failure.literal == e.failure.literal &&
             failure.rule == e.failure.rule && rule == e.rule &&
             token_rule == e.token_rule && choice == e.choice &&
             begin == e.begin && end == e.end && record == e.record;
    }
    size_t hash() const {
      return mix_index(reinterpret_cast<uintptr_t>(failure.literal) ^
                       reinterpret_cast<uintptr_t>(failure.rule) * 3 ^
                       reinterpret_cast<uintptr_t>(rule) * 5 ^
                       reinterpret_cast<uintptr_t>(token_rule) * 7 ^
                       reinterpret_cast<uintptr_t>(choice) * 11 ^ begin * 13 ^
                       record * 17);
    }
  };
  std::vector<ExpectedEvent> events_;

  // The memoized parses in progress, innermost last, and the records of
  // those done: spans of events_, nested or apart. A record noted again, a
  // replay event, comes after its span.
  struct Frame {
    size_t begin;
    size_t generation;
    const Definition *token_rule;
  };
  std::vector<Frame> frames_;
  struct Record {
    size_t begin;
    size_t end;
    const Definition *token_rule;
  };
  std::vector<Record> records_;
  size_t generation_ = 0;
  const char *message_error_pos_ = nullptr; // error_pos when it was given
  bool has_message() const {
    return message_pos && message_error_pos_ == error_pos;
  }
  size_t removed_ = 0; // events remove_repeats has taken out

  // What a memoized parse has noted so far: from its start, or from the
  // start of the generation if it began in an earlier one.
  Record span_of(const Frame &frame) const {
    return {frame.generation == generation_ ? frame.begin : 0, events_.size(),
            frame.token_rule};
  }

  // The records by memo, in open addressing. A slot of an earlier
  // generation is free: its record has gone.
  static constexpr size_t no_record = static_cast<size_t>(-1);
  struct RecordSlot {
    RuleCall key;
    size_t record;
    size_t generation;
  };
  std::vector<RecordSlot> record_slots_;
  size_t indexed_ = 0; // slots of this generation

  // The slot of `key` in this generation, or the free one it would take.
  size_t slot_of(const RuleCall &key) const {
    auto mask = record_slots_.size() - 1;
    auto i = mix_index(reinterpret_cast<uintptr_t>(key.pos) * 31 +
                       reinterpret_cast<uintptr_t>(key.rule) +
                       key.skips_no_whitespace) &
             mask;
    while (record_slots_[i].generation == generation_ &&
           !(record_slots_[i].key == key)) {
      i = (i + 1) & mask;
    }
    return i;
  }

  void keep_record(const RuleCall &key, size_t record) {
    if ((indexed_ + 1) * 2 > record_slots_.size()) {
      auto old = std::move(record_slots_);
      record_slots_.assign(old.empty() ? 16 : old.size() * 2,
                           {{}, no_record, no_record});
      indexed_ = 0;
      for (const auto &slot : old) {
        if (slot.generation == generation_) {
          keep_record(slot.key, slot.record);
        }
      }
    }
    auto &slot = record_slots_[slot_of(key)];
    if (slot.generation != generation_) { indexed_++; }
    slot = {key, record, generation_};
  }

  // A parse that keeps failing at one position notes the same few events
  // over and over. Once this many, or twice what the last removal left, have
  // piled up since a memoized parse last began or ended, their repeats go:
  // each span holds all of them or none.
  static constexpr size_t max_events_ = 256;
  size_t boundary_ = 0;
  size_t remove_repeats_at_ = max_events_;

  void note(const ExpectedEvent &event) {
    if (events_.size() - boundary_ == remove_repeats_at_) { remove_repeats(); }
    events_.push_back(event);
  }
  void remove_repeats();

  void next_generation() {
    events_.clear();
    records_.clear();
    indexed_ = 0;
    boundary_ = 0;
    remove_repeats_at_ = max_events_;
    generation_++;
  }

  void insert_failure(const char *literal, const Definition *rule,
                      const Definition *token_rule);
  // The rules whose start failures have been inserted, with the token rule
  // around them: what they insert depends on nothing else.
  using Expanded =
      std::vector<std::pair<const Definition *, const Definition *>>;
  void insert_start_failure(const StartFailure &failure, const Definition *rule,
                            const Definition *token_rule, Expanded &expanded);

  // Copies of the literals that the grammar does not own. Held by pointer so
  // that their text stays put as this vector grows, and is shared with the
  // copies of this ErrorInfo (a parse result is one).
  std::vector<std::shared_ptr<const std::string>> literal_copies_;

  const char *copy_of(const char *literal) {
    for (const auto &copy : literal_copies_) {
      if (*copy == literal) { return copy->c_str(); }
    }
    literal_copies_.push_back(std::make_shared<const std::string>(literal));
    return literal_copies_.back()->c_str();
  }

  int cast_char(char c) const { return static_cast<unsigned char>(c); }

  std::string heuristic_error_token(const char *s, size_t n,
                                    const char *pos) const {
    auto len = n - std::distance(s, pos);
    if (len) {
      size_t i = 0;
      auto c = cast_char(pos[i++]);
      if (!std::ispunct(c) && !std::isspace(c)) {
        while (i < len && !std::ispunct(cast_char(pos[i])) &&
               !std::isspace(cast_char(pos[i]))) {
          i++;
        }
      }

      size_t count = CPPPEGLIB_HEURISTIC_ERROR_TOKEN_MAX_CHAR_COUNT;
      size_t j = 0;
      while (count > 0 && j < i) {
        j += codepoint_or_byte_length(&pos[j], i - j);
        count--;
      }

      return escape_characters(pos, j);
    }
    return std::string();
  }

  std::string replace_all(std::string str, const std::string &from,
                          const std::string &to) const {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
      str.replace(pos, from.length(), to);
      pos += to.length();
    }
    return str;
  }
};

/*
 * Context
 */
class Ope;

using TracerEnter = std::function<void(
    const Ope &name, const char *s, size_t n, const SemanticValues &vs,
    const Context &c, const std::any &dt, std::any &trace_data)>;

using TracerLeave = std::function<void(
    const Ope &ope, const char *s, size_t n, const SemanticValues &vs,
    const Context &c, const std::any &dt, size_t, std::any &trace_data)>;

using TracerStartOrEnd = std::function<void(std::any &trace_data)>;

// Semantic values of memoized parse results: open-addressing hash map keyed
// by the index of the cache tables (Context::cache_index). Match lengths live
// in a dense per-index array in Context (validity is tracked by its
// registered/success bitvectors), so this map only holds the entries whose
// result carries a std::any value — a grammar without semantic actions never
// allocates it. Keys are probed linearly in a flat array.
class PackratCache {
public:
  explicit PackratCache(size_t expected_entries) {
    while (initial_capacity_ < expected_entries) {
      initial_capacity_ *= 2;
    }
  }

  bool find(size_t key, std::any &val) const {
    if (keys_.empty()) { return false; }
    auto mask = keys_.size() - 1;
    auto i = mix_index(key) & mask;
    while (true) {
      if (keys_[i] == key) {
        val = vals_[i];
        return true;
      }
      if (keys_[i] == kEmpty) { return false; }
      i = (i + 1) & mask;
    }
  }

  void insert_or_assign(size_t key, const std::any &val) {
    if (keys_.empty() || (used_ + 1) * 4 > keys_.size() * 3) { grow(); }
    auto mask = keys_.size() - 1;
    auto i = mix_index(key) & mask;
    while (keys_[i] != key && keys_[i] != kEmpty) {
      i = (i + 1) & mask;
    }
    if (keys_[i] == kEmpty) { used_++; }
    keys_[i] = key;
    vals_[i] = val;
  }

private:
  static constexpr size_t kEmpty = static_cast<size_t>(-1);

  void grow() {
    auto new_cap = keys_.empty() ? initial_capacity_ : keys_.size() * 2;
    std::vector<size_t> old_keys = std::move(keys_);
    std::vector<std::any> old_vals = std::move(vals_);
    keys_.assign(new_cap, kEmpty);
    vals_.assign(new_cap, std::any());
    used_ = 0;
    auto mask = new_cap - 1;
    for (size_t j = 0; j < old_keys.size(); j++) {
      if (old_keys[j] == kEmpty) { continue; }
      auto i = mix_index(old_keys[j]) & mask;
      while (keys_[i] != kEmpty) {
        i = (i + 1) & mask;
      }
      keys_[i] = old_keys[j];
      vals_[i] = std::move(old_vals[j]);
      used_++;
    }
  }

  size_t initial_capacity_ = 1024;
  std::vector<size_t> keys_;
  std::vector<std::any> vals_;
  size_t used_ = 0; // occupied slots
};

/*
 * StartRuleAnalysis
 */

// Whatever depends on the start rule lives here, never on the shared rules.
struct StartRuleAnalysis {
  std::vector<Definition *> rules;
  std::vector<Definition *> rules_by_id;
  bool has_cut = false;
  bool has_user = false;
  std::vector<int32_t> packrat_index; // def_id -> cache slot or -1
  size_t packrat_cached_count = 0;
  // The rules that the alternatives of choices start with.
  std::vector<const Definition *> alternative_starts;

  bool indexes(const Definition &rule) const;
};

// What a rule match goes by to leave out what goes unnoticed: building a
// value that nothing reads (Holder::parse_core), entering a rule that cannot
// start (Reference::parse_dispatch), trying an alternative that cannot start
// (PrioritizedChoice::parse_core), and parsing a rule again where errors are
// muted (Context::makes_packrat_memo).
struct RuleUse {
  // Whether a match is only recognized: it runs no callback, forms no scope
  // and builds no value.
  enum class Recognized : uint8_t {
    never,
    where_unread, // where the scope it is delivered to holds no rule values
    always
  };
  // Whether the scope of a match that is built holds rule values.
  enum class ChildValues : uint8_t {
    read,
    unread,
    passed_up // the first is the rule's own value: read where that is
  };
  Recognized recognized = Recognized::never;
  ChildValues child_values = ChildValues::read;
  bool value_always_empty = false;
  // Whether entering the rule on a byte it cannot start with would run no
  // callback.
  bool skippable = false;
  // Whether trying the rule where it cannot start can do more than its
  // first set says: a rule it enters there gives a message, runs a predicate
  // on an empty match or is left-recursive. Where skipping could be noticed,
  // such a rule is entered, and an alternative that starts with one is tried,
  // instead of skipped.
  bool must_be_tried = true;
  // Whether the rule is parsed only where errors are muted, so that a memo
  // that notes nothing is reused only where nothing is noted either
  // (Context::makes_packrat_memo).
  bool parsed_only_where_muted = false;
};

// The RuleUse of each rule, by id, and whether some alternative starts with
// a rule that must be tried.
struct RuleUses {
  std::vector<RuleUse> by_id;
  bool some_must_be_tried = true;
};

class Context {
public:
  const char *path;
  const char *s;
  const size_t l;

  ErrorInfo error_info;
  bool recovered = false;
  // Where each recovery has reported what was pending, if anything. Parsing
  // a rule again after
  // backtracking runs a recovery in it again where it ran, and reusing the
  // rule's memo does not: either way, it reports nothing there again.
  std::set<std::pair<const Ope *, const char *>> reported_recoveries;

  std::vector<std::unique_ptr<SemanticValues>> value_stack;
  size_t value_stack_size = 0;

  std::vector<Definition *> rule_stack;
  // Where the outermost token rule on rule_stack is, if there is one: a
  // failure inside it is reported under its name.
  size_t outer_token_rule = static_cast<size_t>(-1);

  const Definition *current_rule() const {
    return rule_stack.empty() ? nullptr : rule_stack.back();
  }
  const Definition *token_rule() const {
    return outer_token_rule < rule_stack.size() ? rule_stack[outer_token_rule]
                                                : nullptr;
  }

  // `rule` is not entered at `a_s`, where it cannot start: this notes the
  // failures trying it would.
  void note_untried(const Definition &rule, const char *a_s) {
    if (collects_expected_tokens()) {
      note_failure(a_s, {nullptr, &rule}, false);
    }
  }

  void push_rule(Definition *rule);
  void pop_rule() {
    rule_stack.pop_back();
    if (outer_token_rule == rule_stack.size()) {
      outer_token_rule = static_cast<size_t>(-1);
    }
  }

  std::shared_ptr<Ope> whitespaceOpe;
  bool in_whitespace = false;

  // No whitespace is skipped after a match in a token, a no_whitespace rule
  // or the whitespace, so a rule may match differently there than at the
  // same position elsewhere, and a call of it there is one of its own
  // (RuleCall). Only a parse with whitespace sets this.
  bool skips_no_whitespace = false;

  std::shared_ptr<Ope> wordOpe;

  std::vector<std::pair<std::string_view, std::string>> capture_entries;

  // False when the grammar contains no Cut or Recovery ope (determined once
  // at id-assignment time); lets choices and predicates skip all cut_stack
  // work.
  const bool has_cut;
  // Not std::vector<bool>: GCC 12 to 14 drop its pop_back from a scope_exit
  // on the path an exception takes.
  std::vector<char> cut_stack;

  const size_t def_count;
  const StartRuleAnalysis *const analysis;
  const bool enablePackratParsing;
  const std::vector<int32_t> *packrat_index; // def_id -> cache slot or -1
  size_t packrat_cached_count;               // number of memoized rules
  std::vector<bool> cache_registered;
  std::vector<bool> cache_success;
  // Match length per cache_index(), like the bitvectors above. Left
  // uninitialized on purpose: a slot is only read once cache_success marks
  // it, which happens after it is written.
  std::unique_ptr<uint32_t[]> cache_len;

  PackratCache cache_values;

  // What a match of a rule produced: its length, and the parts a caller may
  // read.
  struct RuleMatch {
    size_t len = kFail;
    std::any val;
    std::string_view token;
  };

  // Left recursion support
  // Rules being parsed, by skips_no_whitespace. A rule with an id only needs
  // its innermost start there, since a nested call never starts before its
  // caller. A rule without one is kept by its calls.
  std::vector<const char *> active_pos[2];
  std::set<RuleCall> in_progress;

  // A packrat memo stands in for parsing the rule again, the failures it
  // notes included. The whitespace notes no failure, so a parse in it makes
  // no memo, though it reuses those of the tokens. Nor does a parse where
  // errors are muted, unless the rule is parsed only where they are. (A
  // left-recursive rule, which needs its matches to grow in linear time,
  // keeps them otherwise: see LRMemo.)
  bool makes_packrat_memo(const RuleUse &use) const {
    return !in_whitespace && (!mutes_errors || use.parsed_only_where_muted);
  }

  RuleCall call_of(const Holder &rule, const char *a_s) const {
    return {&rule, a_s, skips_no_whitespace};
  }

  // Parses what the memo of `call` will hold, and keeps what the parse notes
  // with the memo (ErrorInfo::open_frame) if it makes one. In the parse, a
  // failure is named by the outermost token rule in it.
  template <typename T> void parse_memoized(const RuleCall &call, T parse) {
    auto keeps = collects_expected_tokens();
    auto outer = outer_token_rule;
    if (keeps) {
      error_info.open_frame(token_rule());
      outer_token_rule = static_cast<size_t>(-1);
    }
    parse();
    if (keeps) {
      outer_token_rule = outer;
      error_info.close_frame(call);
    }
  }

  static constexpr size_t no_growth = static_cast<size_t>(-1);

  // Confirmed matches of left-recursive rules. One built from the seed of a
  // rule growing around it, growing[built_from], goes when that seed changes
  // or stops growing. A match is kept wherever it is parsed: each round of
  // growing a rule parses the rules below it again at its position, and
  // without their matches would grow them again. One parsed where no failure
  // is noted (notes_failures) notes none, so it is reused only where none
  // are noted either, and growing the rule again where they are replaces it.
  struct LRMemo {
    RuleMatch match;
    size_t built_from;
    bool notes_failures;
  };
  std::map<RuleCall, LRMemo> lr_memo;

  // The left-recursive rules growing their seeds, innermost last. A rule
  // parsed inside another starts at or after it, so the ones growing where
  // the parse is are on top. A rule that reaches a seed at its own start is
  // in its cycle, which left recursion detection marks, so only the matches
  // of left-recursive rules are taken to be built from a seed.
  struct Growth {
    RuleCall call;
    RuleMatch seed;
    // The innermost growth around this one whose seed its parse used.
    size_t built_from;
    // The confirmed matches built from this seed.
    std::vector<RuleCall> built;
  };
  std::vector<Growth> growing;

  size_t growth_of(const RuleCall &call) const {
    for (auto i = growing.size(); i-- > 0 && growing[i].call.pos == call.pos;) {
      if (growing[i].call == call) { return i; }
    }
    return no_growth;
  }

  // What the rules growing inside growing[i] match is built from its seed.
  void use_seed(size_t i) {
    for (auto j = i + 1; j < growing.size(); j++) {
      auto &built_from = growing[j].built_from;
      if (built_from == no_growth || built_from < i) { built_from = i; }
    }
  }

  void forget_built(size_t i) {
    for (const auto &call : growing[i].built) {
      lr_memo.erase(call);
    }
    growing[i].built.clear();
  }

  // The instances of the macros this parse calls: their bodies with the
  // parameters replaced by the arguments, keyed by macro_inst_key. Released
  // last made first: a later one may take an argument that an earlier one
  // made, and releasing the earlier ones first would leave a chain of such
  // arguments (`M(s) <- M(s / 'x')`) to be freed recursively at once.
  struct Instance {
    const Holder *body = nullptr;
    // Where it is being parsed, by skips_no_whitespace, unless it is
    // left-recursive.
    const char *active_pos[2] = {};
  };
  Instance &instance(const Reference &call);
  std::vector<std::shared_ptr<Holder>> instances;
  std::map<std::vector<const void *>, Instance> instance_by_key;
  std::unordered_map<const Reference *, Instance *> instance_of_call;

  bool has_id(const Definition &rule) const {
    return analysis && analysis->indexes(rule);
  }

  // Map a def_id to its slot in the cache tables, or -1 for rules that are
  // not memoized.
  int32_t cache_slot(size_t def_id) const {
    if (!packrat_index) { return -1; }
    assert(def_id < packrat_index->size());
    return (*packrat_index)[def_id];
  }

  // A rule or a macro instance entered again where it is being parsed is on
  // left recursion that detection missed, and fails there, so a result then
  // depends on what is being parsed around it. No match is reused from then
  // on.
  bool enters_again(bool again) {
    if (again) { packrat_index = nullptr; }
    return again;
  }
  // `active` is where the innermost call in progress started, which a nested
  // call never starts before.
  bool enters_again(const char *active, const char *a_s) {
    assert(!active || active <= a_s);
    return enters_again(active == a_s);
  }

  // The cache tables have a run of entries per memoized rule, one for each
  // position, and with whitespace a second run for the matches where none is
  // skipped. A rule that rarely succeeds leaves the pages of its run in
  // cache_len untouched.
  size_t cache_size() const {
    return packrat_cached_count * (whitespaceOpe ? 2 : 1) * (l + 1);
  }

  size_t cache_index(int32_t slot, const char *pos) const {
    assert(!skips_no_whitespace || whitespaceOpe);
    auto run = static_cast<size_t>(slot) +
               (skips_no_whitespace ? packrat_cached_count : 0);
    return run * (l + 1) + static_cast<size_t>(pos - s);
  }

  void write_packrat_cache(size_t idx, size_t len, const std::any &val) {
    if (sizeof(size_t) > sizeof(uint32_t) &&
        len > static_cast<size_t>(UINT32_MAX)) {
      // A match too long for the 32-bit memo is not memoized, so the rule
      // simply re-parses at this position.
      return;
    }
    cache_registered[idx] = true;
    cache_success[idx] = true;
    cache_len[idx] = static_cast<uint32_t>(len);
    if (val.has_value()) { cache_values.insert_or_assign(idx, val); }
  }

  TracerEnter tracer_enter;
  TracerLeave tracer_leave;
  const bool has_tracer;
  std::any trace_data;
  const bool verbose_trace;

  // The rule matches in progress, and the right operands a precedence rule
  // is parsing. Counted only under a limit.
  size_t depth = 0;
  size_t max_depth = std::numeric_limits<size_t>::max();

  bool limits_depth() const {
    return max_depth != std::numeric_limits<size_t>::max();
  }

  // One level deeper until the result goes out of scope. Too deep a nesting
  // abandons the whole parse: failing this match instead would let the parse
  // go on by backtracking, possibly to a different result.
  auto nest(const char *pos, const Definition &rule) {
    if (limits_depth() && ++depth > max_depth) {
      throw NestingTooDeep{pos, &rule};
    }
    return scope_exit([this]() {
      if (limits_depth()) { depth--; }
    });
  }

  Log log;
  ErrorReporter error_reporter;
  // Whether someone listens to errors.
  const bool reports_errors;
  // Nothing is reported from the operand of `!` or the expression of a
  // recovery: what they fail to match is not what the input lacks.
  bool mutes_errors = false;
  bool reports_here() const { return reports_errors && !mutes_errors; }
  auto mute_errors() {
    return scope_exit([this, was = std::exchange(mutes_errors, true)]() {
      mutes_errors = was;
    });
  }

  // True when error reporting or tracing is active, the only readers of
  // rule_stack.
  const bool needs_rule_stack;

  // By rule id. A rule without an entry is always entered and built.
  const RuleUse *rule_use = nullptr;

  // Whether skipping what must be tried could be noticed: errors are
  // reported or nesting is limited. It holds for the whole parse, so that a
  // recovery, which reports nothing, leaves the memos a parse without
  // shortcuts would.
  bool notices_skipping = false;
  // Whether a choice checks each alternative for one that must be tried:
  // skipping could be noticed, and some alternative starts with such a rule.
  bool checks_alternatives = false;
  bool must_be_tried(const FirstSet &fs) const;

  // The body of a match that is only recognized parses into this scope.
  SemanticValues unread_scope;

  // A tracer and a User operator see the scopes as they are, so every match
  // is built and every value delivered.
  bool builds_everything = false;

  // Whether a rule that cannot start with the next byte may fail without
  // being entered, noting what entering it would. It is entered all the same
  // for a tracer to see it.
  bool skips_rules = false;

  // Until the returned guard goes, neither the values nor the tokens of
  // rule matches are delivered to `vs`: what is parsed is thrown away.
  auto unread_in(SemanticValues &vs) {
    auto se = scope_exit([&vs, values = vs.holds_rule_values_,
                          tokens = vs.holds_rule_tokens_]() {
      vs.holds_rule_values_ = values;
      vs.holds_rule_tokens_ = tokens;
    });
    if (!builds_everything) {
      vs.holds_rule_values_ = false;
      vs.holds_rule_tokens_ = false;
    }
    return se;
  }

  // Parses with `parse` into a scope of its own, thrown away with what it
  // holds.
  template <typename T> size_t parse_thrown_away(T parse) {
    auto &vs = push_semantic_values_scope();
    auto se = scope_exit([&]() { pop_semantic_values_scope(); });
    auto unread = unread_in(vs);
    return parse(vs);
  }

  Context(const char *path, const char *s, size_t l,
          const StartRuleAnalysis *analysis, std::shared_ptr<Ope> whitespaceOpe,
          std::shared_ptr<Ope> wordOpe, bool enablePackratParsing,
          TracerEnter tracer_enter, TracerLeave tracer_leave,
          std::any trace_data, bool verbose_trace, Log log,
          ErrorReporter error_reporter = nullptr)
      : path(path), s(s), l(l), whitespaceOpe(whitespaceOpe), wordOpe(wordOpe),
        has_cut(analysis ? analysis->has_cut : true),
        def_count(analysis ? analysis->rules_by_id.size() : 0),
        analysis(analysis), enablePackratParsing(enablePackratParsing),
        packrat_index(analysis && enablePackratParsing
                          ? &analysis->packrat_index
                          : nullptr),
        packrat_cached_count(packrat_index ? analysis->packrat_cached_count
                                           : 0),
        cache_registered(cache_size()), cache_success(cache_size()),
        cache_len(this->packrat_cached_count ? new uint32_t[cache_size()]
                                             : nullptr),
        cache_values(packrat_index ? l / 8 + 16 : 0),
        active_pos{std::vector<const char *>(def_count),
                   std::vector<const char *>(def_count)},
        tracer_enter(tracer_enter), tracer_leave(tracer_leave),
        has_tracer(tracer_enter && tracer_leave), trace_data(trace_data),
        verbose_trace(verbose_trace), log(log), error_reporter(error_reporter),
        reports_errors(log || error_reporter),
        needs_rule_stack(tracer_enter || tracer_leave || reports_errors),
        unread_scope(this) {
    unread_scope.path = path;
    unread_scope.ss = s;
    unread_scope.holds_rule_values_ = false;
    unread_scope.is_read_ = false;
  }

  ~Context() {
    while (!instances.empty()) {
      instances.pop_back();
    }
    assert(!value_stack_size);
    assert(cut_stack.empty());
  }

  Context(const Context &) = delete;
  Context(Context &&) = delete;
  Context operator=(const Context &) = delete;

  // A context to parse on the side with: it reads the input of `c`, and
  // reports and traces nothing.
  static Context detached(const Context &c) {
    return Context(nullptr, c.s, c.l, nullptr, nullptr, nullptr, false, nullptr,
                   nullptr, nullptr, false, nullptr);
  }

  // Keeps what is parsed in its scope out of the trace, unless it is verbose.
  struct TraceMute {
    explicit TraceMute(Context &c) : c_(c), save_(c.ignore_trace_state) {
      c.ignore_trace_state = !c.verbose_trace;
    }
    ~TraceMute() { c_.ignore_trace_state = save_; }
    TraceMute(const TraceMute &) = delete;
    TraceMute &operator=(const TraceMute &) = delete;

  private:
    Context &c_;
    bool save_;
  };

  // Whether a message at `pos` is reported: a failure here is noted, and no
  // message given for the same error lies at or past `pos`. So nothing in the
  // whitespace gives one, as a rule there fails wherever the whitespace stops.
  bool reports_message_at(const char *pos) const {
    return collects_expected_tokens() &&
           !error_info.message_lies_at_or_after(pos);
  }

  // A failure at `pos` gives the message `make` returns where the failure is
  // the error: what it noted since `mark` is at the furthest failure, or
  // `pos` is at or past it and becomes the error.
  template <typename T>
  void give_message(const char *pos, const ErrorInfo::Mark &mark, T make,
                    const std::string &label = "") {
    if (reports_message_at(pos) &&
        (error_info.noted_since(mark) || error_info.advance_to(pos))) {
      error_info.set_message(pos, make(), label);
    }
  }

  // When the returned guard goes, a cut or a recovery made meanwhile has
  // committed no choice entered before: a predicate's operand leaves nothing
  // but its result. Such a cut marks the innermost choice, which the guard
  // puts back as it was; a choice entered meanwhile has a mark of its own.
  auto contain_cuts() {
    const auto choices = has_cut ? cut_stack.size() : 0;
    const auto marked = choices && cut_stack[choices - 1];
    return scope_exit([this, choices, marked]() {
      if (choices) { cut_stack[choices - 1] = marked; }
    });
  }

  // Per-rule packrat stats (populated when packrat_stats is non-null)
  struct PackratStats {
    size_t hits = 0;
    size_t misses = 0;
  };
  std::vector<PackratStats> *packrat_stats = nullptr;

  // What a rule that is not left-recursive matches at a position. Entered
  // again where it is being parsed, it fails instead of recursing forever; a
  // memoized match is reused, and otherwise `parse` makes the match. The
  // packrat memo holds no token, so a match whose token is needed is parsed.
  template <typename T>
  void reuse_or_parse(const char *a_s, const Definition &rule, bool rule_has_id,
                      bool needs_token, RuleMatch &match, T parse);

  // What a left-recursive rule matches at a position. Entered again where it
  // is being parsed, it gets the match found so far, its seed, and it is
  // parsed again for as long as that makes the seed grow. Its confirmed match
  // is reused, but one built from the seed of a rule growing around it only
  // while that rule grows the same seed.
  template <typename T>
  void reuse_or_grow(const char *a_s, const Holder &rule, RuleMatch &match,
                     T parse);

  // Semantic values
  SemanticValues &push_semantic_values_scope() {
    assert(value_stack_size <= value_stack.size());
    if (value_stack_size == value_stack.size()) {
      // path and ss are fixed for the whole parse, so a frame only needs
      // them set once, on creation.
      value_stack.emplace_back(std::make_unique<SemanticValues>(this));
      auto &vs = *value_stack[value_stack_size++];
      vs.path = path;
      vs.ss = s;
      return vs;
    }

    auto &vs = *value_stack[value_stack_size++];
    if (!vs.empty()) {
      vs.clear();
      if (!vs.tags.empty()) { vs.tags.clear(); }
    }
    vs.sv_ = std::string_view();
    vs.choice_count_ = 0;
    vs.choice_ = 0;
    vs.holds_rule_values_ = true;
    vs.holds_rule_tokens_ = false;
    if (!vs.tokens.empty()) { vs.tokens.clear(); }
    return vs;
  }

  void pop_semantic_values_scope() { value_stack_size--; }

  // Snapshot/Rollback
  struct Snapshot {
    size_t sv_size;
    size_t sv_tags_size;
    size_t sv_tokens_size;
    std::string_view sv_sv;
    size_t choice_count;
    size_t choice;
    size_t capture_size;
  };

  Snapshot snapshot(const SemanticValues &vs) const {
    return {vs.size(),        vs.tags.size(), vs.tokens.size(),      vs.sv_,
            vs.choice_count_, vs.choice_,     capture_entries.size()};
  }

  // Mostly there is nothing to drop, and resize is an out-of-line call.
  template <typename T> static void truncate(T &v, size_t size) {
    if (v.size() > size) { v.resize(size); }
  }

  void rollback(SemanticValues &vs, const Snapshot &snap) {
    truncate(vs, snap.sv_size);
    truncate(vs.tags, snap.sv_tags_size);
    truncate(vs.tokens, snap.sv_tokens_size);
    vs.sv_ = snap.sv_sv;
    vs.choice_count_ = snap.choice_count;
    vs.choice_ = snap.choice;
    truncate(capture_entries, snap.capture_size);
  }

  // Skip trailing whitespace with trace suppression.
  // Returns whitespace length, or -1 on failure.
  // No-op (returns 0) where skips_no_whitespace or without whitespaceOpe.
  size_t skip_whitespace(const char *a_s, size_t n, SemanticValues &vs,
                         std::any &dt);

  // Parses `ope` where no whitespace is skipped.
  size_t parse_without_whitespace(const Ope &ope, const char *a_s, size_t n,
                                  SemanticValues &vs, std::any &dt);

  // Error
  // Where a failure is noted if errors are reported, which does not depend on
  // whether they are, so that the same rules are entered either way. What
  // skipping whitespace fails to match is not what the input lacks.
  bool notes_failures() const { return !mutes_errors && !in_whitespace; }
  bool collects_expected_tokens() const {
    return reports_errors && notes_failures();
  }
  void set_error_pos(const char *a_s, const char *literal = nullptr,
                     bool copy_literal = false) {
    if (collects_expected_tokens()) {
      note_failure(a_s, {literal, nullptr}, literal && copy_literal);
    }
  }
  // Out of line, so that set_error_pos is small enough to be inlined into
  // the failures of a parse that collects nothing.
  void note_failure(const char *a_s, StartFailure failure, bool copy_literal);

  // Trace
  void trace_enter(const Ope &ope, const char *a_s, size_t n,
                   const SemanticValues &vs, std::any &dt);
  void trace_leave(const Ope &ope, const char *a_s, size_t n,
                   const SemanticValues &vs, std::any &dt, size_t len);
  bool is_traceable(const Ope &ope) const;

  // Line info
  std::pair<size_t, size_t> line_info(const char *cur) const {
    // A Context belongs to one parse on one thread, so a plain flag is
    // enough here; std::call_once would pay for an atomic on every node.
    if (!source_line_index_ready_) {
      for (size_t pos = 0; pos < l; pos++) {
        if (s[pos] == '\n') { source_line_index.push_back(pos); }
      }
      source_line_index.push_back(l);
      source_line_index_ready_ = true;
    }

    auto pos = static_cast<size_t>(std::distance(s, cur));

    // Callers walk the input in bursts (building an AST asks for one
    // position per node), so the previous answer is usually still the right
    // line. Checking it turns most lookups into two compares instead of a
    // binary search over every line start in the input.
    auto id = line_info_hint_;
    if (!(id < source_line_index.size() && pos <= source_line_index[id] &&
          (id == 0 || pos > source_line_index[id - 1]))) {
      auto it = std::lower_bound(
          source_line_index.begin(), source_line_index.end(), pos,
          [](size_t element, size_t value) { return element < value; });
      id = static_cast<size_t>(std::distance(source_line_index.begin(), it));
      line_info_hint_ = id;
    }

    auto off = pos - (id == 0 ? 0 : source_line_index[id - 1] + 1);
    return std::pair(id + 1, off + 1);
  }

  size_t next_trace_id = 0;
  std::vector<size_t> trace_ids;
  bool ignore_trace_state = false;
  mutable bool source_line_index_ready_ = false;
  mutable std::vector<size_t> source_line_index;
  mutable size_t line_info_hint_ = 0;
};

/*
 * Parser operators
 */
class Ope {
public:
  struct Visitor;

  virtual ~Ope() = default;
  size_t parse(const char *s, size_t n, SemanticValues &vs, Context &c,
               std::any &dt) const;
  virtual size_t parse_core(const char *s, size_t n, SemanticValues &vs,
                            Context &c, std::any &dt) const = 0;
  virtual void accept(Visitor &v) = 0;

  bool is_token_boundary = false;
  bool is_choice_like = false;

private:
  size_t parse_traced(const char *s, size_t n, SemanticValues &vs, Context &c,
                      std::any &dt) const;
};

class Sequence : public Ope {
public:
  template <typename... Args>
  Sequence(const Args &...args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...} {}
  Sequence(const std::vector<std::shared_ptr<Ope>> &opes) : opes_(opes) {}
  Sequence(std::vector<std::shared_ptr<Ope>> &&opes) : opes_(std::move(opes)) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    size_t i = 0;
    for (const auto &ope : opes_) {
      auto len = ope->parse(s + i, n - i, vs, c, dt);
      if (fail(len)) { return len; }
      i += len;
    }
    return i;
  }

  void accept(Visitor &v) override;

  std::vector<std::shared_ptr<Ope>> opes_;
};

// Whether a parse skips the alternatives of a choice and the rules that
// cannot start with the next byte, and matches a repeated character class in
// one go. The test suite turns these shortcuts off to check that they go
// unnoticed.
#ifdef CPPPEGLIB_TEST
inline bool &takes_shortcuts() {
  static thread_local bool takes = true;
  return takes;
}
#else
constexpr bool takes_shortcuts() { return true; }
#endif

// The bytes a match of an expression can start with. On any other byte, the
// expression does the same: it matches empty or fails, and either way notes
// `failures`, as a parse that tried it would. An expression for which that
// cannot be said can start with any byte. What trying a rule there does that
// depends on the parse, such as giving a message, is left to
// RuleUse::must_be_tried. Used to skip what cannot match without trying it.
//
// An expression that matches empty on other bytes can match empty, but not
// the other way round: `&'a'` can, and fails on any byte but 'a'.
struct FirstSet {
  std::bitset<256> chars;
  bool matches_empty_on_other_bytes = false;
  bool any_char = false;
  std::vector<StartFailure> failures;

  bool can_start_with(unsigned char byte) const {
    return any_char || matches_empty_on_other_bytes || chars.test(byte);
  }

  // Merges where `other` can start, not what it notes.
  void merge(const FirstSet &other) {
    chars |= other.chars;
    if (other.matches_empty_on_other_bytes) {
      matches_empty_on_other_bytes = true;
    }
    if (other.any_char) { any_char = true; }
  }
};

class PrioritizedChoice : public Ope {
public:
  template <typename... Args>
  PrioritizedChoice(bool for_label, const Args &...args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...},
        for_label_(for_label) {
    is_choice_like = true;
    list_alternatives();
  }
  PrioritizedChoice(const std::vector<std::shared_ptr<Ope>> &opes)
      : opes_(opes) {
    is_choice_like = true;
    list_alternatives();
  }
  PrioritizedChoice(std::vector<std::shared_ptr<Ope>> &&opes,
                    bool for_label = false)
      : opes_(std::move(opes)), for_label_(for_label) {
    is_choice_like = true;
    list_alternatives();
  }
  // `like` with these alternatives in place of its own, which start as its
  // own do.
  PrioritizedChoice(std::vector<std::shared_ptr<Ope>> &&opes,
                    const PrioritizedChoice &like)
      : opes_(std::move(opes)), for_label_(like.for_label_),
        first_sets_(like.first_sets_), lists_(like.lists_),
        list_begin_(like.list_begin_) {
    is_choice_like = true;
  }

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    size_t len = kFail;

    // A label choice has no flag of its own: a cut in its alternative, or its
    // recovery, commits the choice around it. It clears that flag to tell a
    // cut in the alternative from one before it, so it puts back a cut made
    // before it.
    const auto track_cut = c.has_cut && !for_label_;
    const auto cut_before =
        !track_cut && c.has_cut && !c.cut_stack.empty() && c.cut_stack.back();
    if (track_cut) { c.cut_stack.push_back(false); }
    auto se = scope_exit([&]() {
      if (track_cut) {
        c.cut_stack.pop_back();
      } else if (cut_before) {
        c.cut_stack.back() = true;
      }
    });

    const auto reports = c.collects_expected_tokens();
    const auto skips = n > 0 && first_sets_ && takes_shortcuts();
    const auto checks = skips && c.checks_alternatives;
    const auto byte = static_cast<unsigned char>(n > 0 ? *s : 0);
    const auto list = skips && !checks ? 1 + byte : 0;
    size_t skipped_from = 0;

    const auto *ids = lists_.data();
    const auto end = list_begin_[list + 1];
    for (auto i = list_begin_[list]; i < end; i++) {
      const size_t id = ids[i];
      if (checks && !is_tried(id, byte, c)) { continue; }

      if (reports) { note_skipped(skipped_from, id, s, c); }
      skipped_from = id + 1;

      if (c.has_cut && !c.cut_stack.empty()) { c.cut_stack.back() = false; }

      auto snap = c.snapshot(vs);
      len = opes_[id]->parse(s, n, vs, c, dt);

      if (success(len)) {
        vs.choice_count_ = opes_.size();
        vs.choice_ = id;
        return len;
      }

      c.rollback(vs, snap);

      if (c.has_cut && !c.cut_stack.empty() && c.cut_stack.back()) {
        return len;
      }
    }

    if (reports) { note_skipped(skipped_from, opes_.size(), s, c); }

    return len;
  }

  const std::vector<FirstSet> &first_sets() const {
    static const std::vector<FirstSet> none;
    return first_sets_ ? *first_sets_ : none;
  }

  void accept(Visitor &v) override;

  size_t size() const { return opes_.size(); }

  const std::vector<std::shared_ptr<Ope>> opes_;
  bool for_label_ = false;

private:
  friend struct SetupFirstSets;

  void set_first_sets(std::vector<FirstSet> first_sets) {
    first_sets_ =
        std::make_shared<const std::vector<FirstSet>>(std::move(first_sets));
    list_alternatives();
  }

  // The alternatives to try are read from a list. List 0 has them all. With
  // first sets there is also a list for each byte, of the ones that can
  // start with it.
  void list_alternatives() {
    lists_.clear();
    list_begin_.clear();
    lists_.reserve(opes_.size());
    const size_t list_count = first_sets_ ? 1 + 256 : 1;
    list_begin_.reserve(list_count + 1);

    auto add_list = [&](auto is_listed) {
      list_begin_.push_back(static_cast<uint32_t>(lists_.size()));
      for (size_t id = 0; id < opes_.size(); id++) {
        if (is_listed(id)) { lists_.push_back(static_cast<uint32_t>(id)); }
      }
    };

    add_list([](size_t) { return true; });
    if (first_sets_) {
      for (size_t byte = 0; byte < 256; byte++) {
        add_list([&](size_t id) {
          return (*first_sets_)[id].can_start_with(
              static_cast<unsigned char>(byte));
        });
      }
    }
    list_begin_.push_back(static_cast<uint32_t>(lists_.size()));
    lists_.shrink_to_fit();
  }

  bool is_tried(size_t id, unsigned char byte, const Context &c) const {
    const auto &fs = (*first_sets_)[id];
    return fs.can_start_with(byte) || c.must_be_tried(fs);
  }

  // An error report names what the skipped alternatives expected. Trying
  // any of them would have noted a failure here: what does not match empty
  // there fails at a literal, a class or a dictionary.
  void note_skipped(size_t begin, size_t end, const char *s, Context &c) const {
    if (begin < end && c.error_info.advance_to(s)) {
      c.error_info.add_skipped(this, begin, end, c.current_rule(),
                               c.token_rule());
    }
  }

  // Shared with the choices made like this one.
  std::shared_ptr<const std::vector<FirstSet>> first_sets_;
  std::vector<uint32_t> lists_;
  // Where each list starts in lists_, then where the last one ends.
  std::vector<uint32_t> list_begin_;
};

class Repetition : public Ope {
public:
  // `{n,m}` with m below n is taken as `{n}`.
  Repetition(const std::shared_ptr<Ope> &ope, size_t min, size_t max)
      : ope_(ope), min_(min), max_(std::max(min, max)) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    // ISpan fast path: tight loop for ASCII CharacterClass repetition.
    // Safe because each ASCII match is exactly 1 byte, so byte count == match
    // count.
    if (span_bitset_ && takes_shortcuts()) {
      const auto &bitset = *span_bitset_;
      size_t i = 0;
      if (max_ == std::numeric_limits<size_t>::max()) {
        // Unbounded repetition (*, +): no per-iteration max check
        while (i < n && bitset.test(static_cast<unsigned char>(s[i]))) {
          i++;
        }
      } else {
        auto limit = std::min(n, max_);
        while (i < limit && bitset.test(static_cast<unsigned char>(s[i]))) {
          i++;
        }
      }
      // As matching the class once more would, unless the repetition stopped
      // at its maximum.
      if (i < max_) { c.set_error_pos(s + i); }
      if (i < min_) { return kFail; }
      return i;
    }

    size_t count = 0;
    size_t i = 0;
    while (count < min_) {
      auto len = ope_->parse(s + i, n - i, vs, c, dt);
      if (fail(len)) { return len; }
      i += len;
      count++;
    }

    while (count < max_) {
      auto snap = c.snapshot(vs);
      auto len = ope_->parse(s + i, n - i, vs, c, dt);
      if (fail(len)) {
        c.rollback(vs, snap);
        break;
      }
      i += len;
      count++;
      // A match that consumes nothing would usually match the same way again
      // forever, so it ends the repetition. Loading a grammar checks for such
      // a repetition, but the check is not exhaustive, and a grammar built
      // with combinators is never checked.
      if (len == 0 && max_ == std::numeric_limits<size_t>::max()) { break; }
    }
    return i;
  }

  void accept(Visitor &v) override;

  bool is_zom() const {
    return min_ == 0 && max_ == std::numeric_limits<size_t>::max();
  }

  static std::shared_ptr<Repetition> zom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> oom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 1,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> opt(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0, 1);
  }

  std::shared_ptr<Ope> ope_;
  size_t min_;
  size_t max_;
  const std::bitset<256> *span_bitset_ =
      nullptr; // non-owning, set by SetupFirstSets
};

class AndPredicate : public Ope {
public:
  AndPredicate(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto snap = c.snapshot(vs);
    size_t len;
    {
      auto unread = c.unread_in(vs);
      auto cuts = c.contain_cuts();
      len = ope_->parse(s, n, vs, c, dt);
    }
    c.rollback(vs, snap); // Always rollback — predicates consume nothing
    if (success(len)) {
      return 0;
    } else {
      return len;
    }
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope {
public:
  NotPredicate(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto snap = c.snapshot(vs);
    size_t len;
    {
      auto unread = c.unread_in(vs);
      auto muted = c.mute_errors();
      auto cuts = c.contain_cuts();
      len = ope_->parse(s, n, vs, c, dt);
    }
    c.rollback(vs, snap); // Always rollback — predicates consume nothing
    if (success(len)) {
      c.set_error_pos(s);
      return kFail;
    } else {
      return 0;
    }
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Dictionary : public Ope {
public:
  Dictionary(const std::vector<std::string> &v, bool ignore_case)
      : trie_(v, ignore_case) {
    is_choice_like = true;
  }

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  Trie trie_;
};

class LiteralString : public Ope {
public:
  LiteralString(std::string &&s, bool ignore_case)
      : lit_(std::move(s)), ignore_case_(ignore_case),
        lower_lit_(ignore_case ? to_lower(lit_) : std::string()),
        is_word_(false) {}

  LiteralString(const std::string &s, bool ignore_case)
      : lit_(s), ignore_case_(ignore_case),
        lower_lit_(ignore_case ? to_lower(lit_) : std::string()),
        is_word_(false) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  std::string lit_;
  bool ignore_case_;
  std::string lower_lit_; // pre-computed for ignore_case
  mutable std::once_flag init_is_word_;
  mutable bool is_word_;
};

class CharacterClass : public Ope {
public:
  CharacterClass(const std::string &s, bool negated, bool ignore_case)
      : negated_(negated), ignore_case_(ignore_case) {
    auto chars = decode(s.data(), s.length());
    auto i = 0u;
    while (i < chars.size()) {
      if (i + 2 < chars.size() && chars[i + 1] == '-') {
        auto cp1 = chars[i];
        auto cp2 = chars[i + 2];
        ranges_.emplace_back(std::pair(cp1, cp2));
        i += 3;
      } else {
        auto cp = chars[i];
        ranges_.emplace_back(std::pair(cp, cp));
        i += 1;
      }
    }
    assert(!ranges_.empty());
    setup_ascii_bitset();
  }

  CharacterClass(const std::vector<std::pair<char32_t, char32_t>> &ranges,
                 bool negated, bool ignore_case)
      : ranges_(ranges), negated_(negated), ignore_case_(ignore_case) {
    assert(!ranges_.empty());
    setup_ascii_bitset();
  }

  size_t parse_core(const char *s, size_t n, SemanticValues & /*vs*/,
                    Context &c, std::any & /*dt*/) const override {
    char32_t cp = 0;
    auto len = decode_codepoint(s, n, cp);
    if (len && matches(cp)) { return len; }

    c.set_error_pos(s);
    return kFail;
  }

  void accept(Visitor &v) override;

  friend struct GrammarBlob;
  friend struct OpeSignature;

  bool is_ascii_only() const { return is_ascii_only_; }
  const std::bitset<256> &ascii_bitset() const { return ascii_bitset_; }

private:
  bool in_ranges(char32_t cp) const {
    return std::any_of(ranges_.begin(), ranges_.end(), [cp](const auto &range) {
      return range.first <= cp && cp <= range.second;
    });
  }

  // With `i`, a letter is listed when either of its cases is in the ranges.
  bool matches(char32_t cp) const {
    auto listed = in_ranges(cp) || (ignore_case_ && in_ranges(other_case(cp)));
    return listed != negated_;
  }

  void setup_ascii_bitset() {
    if (negated_) { return; } // negated classes can match non-ASCII
    for (const auto &[lo, hi] : ranges_) {
      if (lo > 0x7F || hi > 0x7F) { return; }
    }
    is_ascii_only_ = true;
    for (char32_t cp = 0; cp <= 0x7F; cp++) {
      if (matches(cp)) { ascii_bitset_.set(cp); }
    }
  }

  std::vector<std::pair<char32_t, char32_t>> ranges_;
  bool negated_;
  bool ignore_case_;
  std::bitset<256> ascii_bitset_;
  bool is_ascii_only_ = false;
};

class Character : public Ope {
public:
  Character(char32_t ch) : ch_(ch) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*vs*/,
                    Context &c, std::any & /*dt*/) const override {
    char32_t cp = 0;
    auto len = decode_codepoint(s, n, cp);

    if (!len || cp != ch_) {
      c.set_error_pos(s);
      return kFail;
    }
    return len;
  }

  void accept(Visitor &v) override;

  char32_t ch_;
};

class AnyCharacter : public Ope {
public:
  size_t parse_core(const char *s, size_t n, SemanticValues & /*vs*/,
                    Context &c, std::any & /*dt*/) const override {
    auto len = codepoint_length(s, n);
    if (len < 1) {
      c.set_error_pos(s);
      return kFail;
    }
    return len;
  }

  void accept(Visitor &v) override;
};

class CaptureScope : public Ope {
public:
  CaptureScope(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto cap_snap = c.capture_entries.size();
    auto len = ope_->parse(s, n, vs, c, dt);
    c.capture_entries.resize(cap_snap); // Always rollback (isolation)
    return len;
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Capture : public Ope {
public:
  using MatchAction = std::function<void(const char *s, size_t n, Context &c)>;

  Capture(const std::shared_ptr<Ope> &ope, MatchAction ma)
      : ope_(ope), match_action_(ma) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto len = ope_->parse(s, n, vs, c, dt);
    if (success(len) && match_action_) { match_action_(s, len, c); }
    return len;
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
  MatchAction match_action_;
};

class TokenBoundary : public Ope {
public:
  TokenBoundary(const std::shared_ptr<Ope> &ope) : ope_(ope) {
    is_token_boundary = true;
  }

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Ignore : public Ope {
public:
  Ignore(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*vs*/,
                    Context &c, std::any &dt) const override {
    return c.parse_thrown_away(
        [&](SemanticValues &vs) { return ope_->parse(s, n, vs, c, dt); });
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

using Parser = std::function<size_t(const char *s, size_t n, SemanticValues &vs,
                                    std::any &dt)>;

class User : public Ope {
public:
  User(Parser fn) : fn_(fn) {}
  size_t parse_core(const char *s, size_t n, SemanticValues &vs,
                    Context & /*c*/, std::any &dt) const override {
    assert(fn_);
    return fn_(s, n, vs, dt);
  }
  void accept(Visitor &v) override;
  std::function<size_t(const char *s, size_t n, SemanticValues &vs,
                       std::any &dt)>
      fn_;
};

class WeakHolder : public Ope {
public:
  WeakHolder(const std::shared_ptr<Ope> &ope) : weak_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto ope = weak_.lock();
    assert(ope);
    return ope->parse(s, n, vs, c, dt);
  }

  void accept(Visitor &v) override;

  std::weak_ptr<Ope> weak_;
};

class Holder : public Ope {
public:
  Holder(Definition *outer) : outer_(outer) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  std::any reduce(SemanticValues &vs, std::any &dt,
                  const std::any &predicate_data) const;

  const std::string &name() const;
  const std::string &trace_name() const;

  std::shared_ptr<Ope> ope_;
  Definition *outer_;
  mutable std::once_flag trace_name_init_;
  mutable std::string trace_name_;

  friend class Definition;

private:
  size_t parse_ope_body(const char *s, size_t n, SemanticValues &vs, Context &c,
                        std::any &dt) const;
};

using Grammar = std::unordered_map<std::string, Definition>;

class Reference : public Ope {
public:
  Reference(const Grammar &grammar, const std::string &name, const char *s,
            bool is_macro, const std::vector<std::shared_ptr<Ope>> &args)
      : grammar_(grammar), name_(name), s_(s), is_macro_(is_macro), args_(args),
        rule_(nullptr), iarg_(0) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  const Grammar &grammar_;
  const std::string name_;
  const char *s_;

  const bool is_macro_;
  const std::vector<std::shared_ptr<Ope>> args_;

  Definition *rule_;
  size_t iarg_;

private:
  size_t parse_dispatch(const char *s, size_t n, SemanticValues &vs, Context &c,
                        std::any &dt) const;
};

class Whitespace : public Ope {
public:
  Whitespace(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    if (c.in_whitespace) { return 0; }
    c.in_whitespace = true;
    auto se = scope_exit([&]() { c.in_whitespace = false; });
    return c.parse_without_whitespace(*ope_, s, n, vs, dt);
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class BackReference : public Ope {
public:
  BackReference(std::string &&name) : name_(std::move(name)) {}

  BackReference(const std::string &name) : name_(name) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  std::string name_;
};

class PrecedenceClimbing : public Ope {
public:
  // Keyed by the operator's text. The keys own it: the grammar text they
  // were read from may be gone by the time of a parse.
  using BinOpeInfo =
      std::map<std::string, std::pair<size_t, char>, std::less<>>;

  PrecedenceClimbing(const std::shared_ptr<Ope> &atom,
                     const std::shared_ptr<Ope> &binop, const BinOpeInfo &info,
                     const Definition &rule)
      : atom_(atom), binop_(binop), info_(info), rule_(rule),
        repetition_(std::make_shared<Sequence>(
            atom, Repetition::zom(std::make_shared<Sequence>(binop, atom)))) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  const std::shared_ptr<Ope> atom_;
  const std::shared_ptr<Ope> binop_;
  BinOpeInfo info_;
  const Definition &rule_;
  // `atom (binop atom)*`, the repetition this parses, which is what the
  // analyses of a grammar look at.
  const std::shared_ptr<Ope> repetition_;

private:
  // Sets operand_missing when an operator is left without its right
  // operand: every enclosing level would read it again and stop there too.
  size_t parse_expression(const char *s, size_t n, SemanticValues &vs,
                          Context &c, std::any &dt, size_t min_prec,
                          bool &operand_missing) const;

  template <typename F>
  static size_t parse_in_scope(SemanticValues &vs, Context &c, F parse);

  // Hands the values of `from` over to `vs`, whether none or several, with
  // their tags.
  static void append_values(SemanticValues &vs, SemanticValues &from) {
    for (auto &v : from) {
      vs.emplace_back(std::move(v));
    }
    vs.tags.insert(vs.tags.end(), from.tags.begin(), from.tags.end());
  }
};

// `%recover(ope)`, and `^label` for `%recover(label)`: where the expression
// it follows fails, it reports the error, with the message of the rule `ope`
// refers to if any, and skips what `ope` matches.
class Recovery : public Ope {
public:
  Recovery(const std::shared_ptr<Ope> &ope)
      : ope_(ope), label_(dynamic_cast<const Reference *>(ope.get())) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override;

  void accept(Visitor &v) override;

  // The rule a `^label` names, which gives the message, if `ope_` refers to
  // one.
  const Definition *label() const { return label_ ? label_->rule_ : nullptr; }

  std::shared_ptr<Ope> ope_;

private:
  const Reference *label_;
};

class Cut : public Ope {
public:
  size_t parse_core(const char * /*s*/, size_t /*n*/, SemanticValues & /*vs*/,
                    Context &c, std::any & /*dt*/) const override {
    if (!c.cut_stack.empty()) { c.cut_stack.back() = true; }
    return 0;
  }

  void accept(Visitor &v) override;
};

/*
 * Factories
 */
template <typename... Args> std::shared_ptr<Ope> seq(Args &&...args) {
  return std::make_shared<Sequence>(static_cast<std::shared_ptr<Ope>>(args)...);
}

template <typename... Args> std::shared_ptr<Ope> cho(Args &&...args) {
  return std::make_shared<PrioritizedChoice>(
      false, static_cast<std::shared_ptr<Ope>>(args)...);
}

template <typename... Args> std::shared_ptr<Ope> cho4label_(Args &&...args) {
  return std::make_shared<PrioritizedChoice>(
      true, static_cast<std::shared_ptr<Ope>>(args)...);
}

inline std::shared_ptr<Ope> zom(const std::shared_ptr<Ope> &ope) {
  return Repetition::zom(ope);
}

inline std::shared_ptr<Ope> oom(const std::shared_ptr<Ope> &ope) {
  return Repetition::oom(ope);
}

inline std::shared_ptr<Ope> opt(const std::shared_ptr<Ope> &ope) {
  return Repetition::opt(ope);
}

inline std::shared_ptr<Ope> rep(const std::shared_ptr<Ope> &ope, size_t min,
                                size_t max) {
  return std::make_shared<Repetition>(ope, min, max);
}

inline std::shared_ptr<Ope> apd(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<AndPredicate>(ope);
}

inline std::shared_ptr<Ope> npd(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<NotPredicate>(ope);
}

inline std::shared_ptr<Ope> dic(const std::vector<std::string> &v,
                                bool ignore_case) {
  return std::make_shared<Dictionary>(v, ignore_case);
}

inline std::shared_ptr<Ope> lit(std::string &&s) {
  return std::make_shared<LiteralString>(std::move(s), false);
}

inline std::shared_ptr<Ope> liti(std::string &&s) {
  return std::make_shared<LiteralString>(std::move(s), true);
}

inline std::shared_ptr<Ope> cls(const std::string &s) {
  return std::make_shared<CharacterClass>(s, false, false);
}

inline std::shared_ptr<Ope>
cls(const std::vector<std::pair<char32_t, char32_t>> &ranges,
    bool ignore_case = false) {
  return std::make_shared<CharacterClass>(ranges, false, ignore_case);
}

inline std::shared_ptr<Ope> ncls(const std::string &s) {
  return std::make_shared<CharacterClass>(s, true, false);
}

inline std::shared_ptr<Ope>
ncls(const std::vector<std::pair<char32_t, char32_t>> &ranges,
     bool ignore_case = false) {
  return std::make_shared<CharacterClass>(ranges, true, ignore_case);
}

inline std::shared_ptr<Ope> chr(char32_t dt) {
  return std::make_shared<Character>(dt);
}

inline std::shared_ptr<Ope> dot() { return std::make_shared<AnyCharacter>(); }

inline std::shared_ptr<Ope> csc(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<CaptureScope>(ope);
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope> &ope,
                                Capture::MatchAction ma) {
  return std::make_shared<Capture>(ope, ma);
}

inline std::shared_ptr<Ope> tok(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<TokenBoundary>(ope);
}

inline std::shared_ptr<Ope> ign(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<Ignore>(ope);
}

inline std::shared_ptr<Ope>
usr(std::function<size_t(const char *s, size_t n, SemanticValues &vs,
                         std::any &dt)>
        fn) {
  return std::make_shared<User>(fn);
}

inline std::shared_ptr<Ope> ref(const Grammar &grammar, const std::string &name,
                                const char *s, bool is_macro,
                                const std::vector<std::shared_ptr<Ope>> &args) {
  return std::make_shared<Reference>(grammar, name, s, is_macro, args);
}

inline std::shared_ptr<Ope> wsp(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<Whitespace>(std::make_shared<Ignore>(ope));
}

inline std::shared_ptr<Ope> bkr(std::string &&name) {
  return std::make_shared<BackReference>(std::move(name));
}

inline std::shared_ptr<Ope> pre(const std::shared_ptr<Ope> &atom,
                                const std::shared_ptr<Ope> &binop,
                                const PrecedenceClimbing::BinOpeInfo &info,
                                const Definition &rule) {
  return std::make_shared<PrecedenceClimbing>(atom, binop, info, rule);
}

inline std::shared_ptr<Ope> rec(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<Recovery>(ope);
}

inline std::shared_ptr<Ope> cut() { return std::make_shared<Cut>(); }

/*
 * Visitor
 */
/*
 * Every operator type, in the order of Ope::Visitor's overloads.
 */
// clang-format off
#define PEGLIB_OPE_TYPES(X) \
  X(Sequence) X(PrioritizedChoice) X(Repetition) \
  X(AndPredicate) X(NotPredicate) X(Dictionary) \
  X(LiteralString) X(CharacterClass) X(Character) \
  X(AnyCharacter) X(CaptureScope) X(Capture) \
  X(TokenBoundary) X(Ignore) X(User) \
  X(WeakHolder) X(Holder) X(Reference) \
  X(Whitespace) X(BackReference) X(PrecedenceClimbing) \
  X(Recovery) X(Cut)
// clang-format on

struct Ope::Visitor {
  virtual ~Visitor() {}
#define PEGLIB_VISIT(T)                                                        \
  virtual void visit(T &) {}
  PEGLIB_OPE_TYPES(PEGLIB_VISIT)
#undef PEGLIB_VISIT
};

// A visitor that must handle every operator type: one that leaves an
// overload out is abstract, so it fails to build instead of skipping the type.
struct StrictVisitor : public Ope::Visitor {
  using Ope::Visitor::visit;
#define PEGLIB_VISIT(T) void visit(T &) override = 0;
  PEGLIB_OPE_TYPES(PEGLIB_VISIT)
#undef PEGLIB_VISIT
};

struct TraversalVisitor : public Ope::Visitor {
  using Ope::Visitor::visit;
  void visit(Sequence &ope) override {
    for (auto &op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto &op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  // A rule made with the combinators refers to itself through a WeakHolder,
  // with no Reference to stop at, so it is not walked into again inside
  // itself. An analysis whose result depends on what such a rule matches
  // treats it as it does a Reference instead.
  void visit(WeakHolder &ope) override {
    auto holder = ope.weak_.lock();
    if (!holder || std::find(rules_in_walk_.begin(), rules_in_walk_.end(),
                             holder.get()) != rules_in_walk_.end()) {
      return;
    }
    rules_in_walk_.push_back(holder.get());
    holder->accept(*this);
    rules_in_walk_.pop_back();
  }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(Recovery &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override {
    ope.repetition_->accept(*this);
  }

  // A walk started inside another goes on from the rules that one is in.
  void walk_within(const TraversalVisitor &outer) {
    rules_in_walk_ = outer.rules_in_walk_;
  }

private:
  std::vector<const Ope *> rules_in_walk_;
};

struct TraceOpeName : public Ope::Visitor {
  using Ope::Visitor::visit;

#define PEGLIB_VISIT(T)                                                        \
  void visit(T &ope) override { name_ = name_of(ope, #T); }
  PEGLIB_OPE_TYPES(PEGLIB_VISIT)
#undef PEGLIB_VISIT

  static std::string get(const Ope &ope) {
    TraceOpeName vis;
    const_cast<Ope &>(ope).accept(vis);
    return vis.name_;
  }

private:
  // A rule is traced by its name, every other operator by its type.
  static const char *name_of(Ope &, const char *type) { return type; }
  static const char *name_of(Holder &ope, const char *) {
    return ope.trace_name().data();
  }

  const char *name_ = nullptr;
};

struct AssignIDToDefinition : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(Holder &ope) override;
  void visit(Reference &ope) override;
  void visit(Cut &) override { has_cut = true; }
  void visit(Recovery &ope) override {
    // Recovery also raises the cut flag on success, so it counts as a cut
    // source for the static cut_stack skip.
    has_cut = true;
    TraversalVisitor::visit(ope);
  }
  void visit(User &) override { has_user = true; }
  void visit(PrioritizedChoice &ope) override {
    for (const auto &fs : ope.first_sets()) {
      for (const auto &f : fs.failures) {
        if (f.rule) { alternative_starts.insert(f.rule); }
      }
    }
    TraversalVisitor::visit(ope);
  }

  std::unordered_map<void *, size_t> ids;
  bool has_cut = false; // grammar contains a Cut or Recovery ope
  bool has_user = false;
  std::unordered_set<const Definition *> alternative_starts;
};

struct IsLiteralToken : public Ope::Visitor {
  using Ope::Visitor::visit;

  void visit(PrioritizedChoice &ope) override {
    for (const auto &op : ope.opes_) {
      if (!IsLiteralToken::check(*op)) { return; }
    }
    result_ = true;
  }

  void visit(Dictionary &) override { result_ = true; }
  void visit(LiteralString &) override { result_ = true; }

  static bool check(Ope &ope) {
    IsLiteralToken vis;
    ope.accept(vis);
    return vis.result_;
  }

private:
  bool result_ = false;
};

struct TokenChecker : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(TokenBoundary &) override { has_token_boundary_ = true; }
  void visit(AndPredicate &) override {}
  void visit(NotPredicate &) override {}
  void visit(WeakHolder &) override { has_rule_ = true; }
  void visit(Reference &ope) override;
  // The operator is a rule of its own, and the atom stands for the match.
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  static bool is_token(Ope &ope) {
    if (IsLiteralToken::check(ope)) { return true; }

    TokenChecker vis;
    ope.accept(vis);
    return vis.has_token_boundary_ || !vis.has_rule_;
  }

private:
  bool has_token_boundary_ = false;
  bool has_rule_ = false;
};

struct DetectLeftRecursion : public TraversalVisitor {
  using TraversalVisitor::visit;

  explicit DetectLeftRecursion(const Definition &rule) : rule_(&rule) {}

  void visit(Sequence &ope) override {
    for (const auto &op : ope.opes_) {
      op->accept(*this);
      if (done_) {
        break;
      } else if (error_s) {
        done_ = true;
        break;
      }
    }
  }
  // A choice consumes input only if each of its alternatives does, and a
  // repetition only if it must repeat something that does.
  void visit(PrioritizedChoice &ope) override {
    auto all_done = true;
    for (const auto &op : ope.opes_) {
      op->accept(*this);
      if (error_s) {
        done_ = true;
        return;
      }
      all_done = all_done && done_;
    }
    done_ = all_done;
  }
  void visit(Repetition &ope) override {
    ope.ope_->accept(*this);
    done_ = done_ && ope.min_ > 0;
  }
  void visit(AndPredicate &ope) override {
    ope.ope_->accept(*this);
    done_ = false;
  }
  void visit(NotPredicate &ope) override {
    ope.ope_->accept(*this);
    done_ = false;
  }
  void visit(Dictionary &) override { done_ = true; }
  void visit(LiteralString &ope) override { done_ = !ope.lit_.empty(); }
  void visit(CharacterClass &) override { done_ = true; }
  void visit(Character &) override { done_ = true; }
  void visit(AnyCharacter &) override { done_ = true; }
  void visit(User &) override { done_ = true; }
  void visit(Reference &ope) override;
  void visit(WeakHolder &ope) override;
  void visit(BackReference &) override { done_ = true; }
  void visit(Cut &) override { done_ = false; }
  // See ComputeCanBeEmpty.
  void visit(Recovery &ope) override {
    ope.ope_->accept(*this);
    if (!error_s) { done_ = true; }
  }

  const char *error_s = nullptr;

  // What a bare parameter reference denotes, plus the frame it was found at
  // -- see visit_in_defining_scope.
  struct ResolvedArg {
    std::shared_ptr<Ope> ope;
    size_t depth = 0;
  };
  ResolvedArg resolve_macro_arg(size_t iarg) const;
  void visit_in_defining_scope(const ResolvedArg &arg);
  void visit_argument(const ResolvedArg &arg);

  // A macro's body depends on its arguments, so "already visited" has to be
  // per instantiation, not per rule: in `A <- W('z') / W(A)`, visiting W with
  // 'z' says nothing about W with A. An instantiation is identified by its
  // arguments, a bare parameter resolved to what it was given. An argument
  // that wraps a parameter, as `x 'a'` in `M(x) <- N(x 'a')`, stands as
  // written, so calls the parse tells apart can share one here: told apart,
  // instantiations could grow without bound.
  size_t intern_macro_inst(const Reference &ope);

  // A macro that instantiates itself with a growing argument
  // (`M(s) <- M(s / 'x')`) has no finite set of instantiations. Past this
  // depth the walk stops descending, and a cycle it then misses fails where
  // the parse enters it again. (A parse of such a growing macro makes
  // instantiations without end, the limit the README states.) The bound is
  // on nesting depth in general, so it caps any chain of nested macro calls,
  // generously for real grammars.
  static const size_t max_macro_inst_depth = 32;

private:
  const Definition *rule_;
  std::set<std::pair<const Definition *, size_t>> refs_;
  std::map<std::vector<const void *>, size_t> macro_inst_ids_;
  std::map<std::vector<const void *>, bool> args_done_;
  size_t next_macro_inst_ = 1;
  bool done_ = false;
  std::vector<const std::vector<std::shared_ptr<Ope>> *> macro_args_stack_;
};

// The rules an operator walks into, each once: a rule made with the
// combinators, which no Reference leads to, is walked into through its
// Holder. A rule comes after those it walks into, unless they reach it.
struct CollectWalkedRules : public TraversalVisitor {
  using TraversalVisitor::visit;
  std::vector<Definition *> rules;

  void visit(Holder &ope) override {
    if (seen_.insert(ope.outer_).second) {
      ope.ope_->accept(*this);
      rules.push_back(ope.outer_);
    }
  }

private:
  std::unordered_set<const Definition *> seen_;
};

struct ComputeCanBeEmpty : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(Sequence &ope) override {
    result_ = std::all_of(ope.opes_.begin(), ope.opes_.end(),
                          [](auto &op) { return check(*op); });
  }
  void visit(PrioritizedChoice &ope) override {
    result_ = std::any_of(ope.opes_.begin(), ope.opes_.end(),
                          [](auto &op) { return check(*op); });
  }
  void visit(Repetition &ope) override {
    if (ope.min_ == 0) {
      result_ = true;
    } else {
      ope.ope_->accept(*this);
    }
  }
  void visit(AndPredicate &) override { result_ = true; }
  void visit(NotPredicate &) override { result_ = true; }
  void visit(Dictionary &) override { result_ = false; }
  void visit(LiteralString &ope) override { result_ = ope.lit_.empty(); }
  void visit(CharacterClass &) override { result_ = false; }
  void visit(Character &) override { result_ = false; }
  void visit(AnyCharacter &) override { result_ = false; }
  void visit(User &) override { result_ = false; }
  void visit(Holder &ope) override;
  void visit(Reference &ope) override;
  void visit(BackReference &) override { result_ = false; }
  void visit(Cut &) override { result_ = true; }
  // A recovery only matches after an error, and a repetition ends on an
  // empty match, so the checks made when a grammar is loaded take it to
  // consume input.
  void visit(Recovery &) override { result_ = false; }

  static bool check(Ope &ope) {
    ComputeCanBeEmpty vis;
    ope.accept(vis);
    return vis.result_;
  }

private:
  bool result_ = false;
};

// Structural signature of an Ope. Two alternatives whose first k elements
// have equal signatures consume the same text, so their (k+1)-th elements
// start at the same position. Opes whose state cannot be serialized get
// their address instead: that only ever reads as "these differ", which
// costs an optimization rather than adding one.
struct OpeSignature : public Ope::Visitor {
  using Ope::Visitor::visit;
  std::string s;

  void visit(Sequence &ope) override { group("seq", ope.opes_); }
  void visit(PrioritizedChoice &ope) override { group("cho", ope.opes_); }
  void visit(Repetition &ope) override {
    s += "(rep " + std::to_string(ope.min_) + " " +
         (ope.max_ == std::numeric_limits<size_t>::max()
              ? std::string("inf")
              : std::to_string(ope.max_));
    wrap(*ope.ope_);
  }
  void visit(AndPredicate &ope) override { unary("and", *ope.ope_); }
  void visit(NotPredicate &ope) override { unary("not", *ope.ope_); }
  void visit(CaptureScope &ope) override { unary("cps", *ope.ope_); }
  void visit(Capture &ope) override { unary("cap", *ope.ope_); }
  void visit(TokenBoundary &ope) override { unary("tok", *ope.ope_); }
  void visit(Ignore &ope) override { unary("ign", *ope.ope_); }
  void visit(Whitespace &ope) override { unary("wsp", *ope.ope_); }
  void visit(Recovery &ope) override { unary("rec", *ope.ope_); }
  // A rule is named, never expanded — that is what keeps a recursive
  // grammar's signature finite. WeakHolder only ever wraps a Holder, so
  // descending through it lands on a name too.
  void visit(Holder &ope) override { s += "(hld " + ope.name() + ")"; }
  void visit(WeakHolder &ope) override {
    if (auto p = ope.weak_.lock()) {
      unary("wek", *p);
    } else {
      opaque(&ope);
    }
  }
  void visit(Reference &ope) override {
    s += "(ref " + ope.name_;
    for (auto &arg : ope.args_) {
      s += ' ';
      arg->accept(*this);
    }
    s += ')';
  }
  void visit(LiteralString &ope) override {
    s += "(lit " + std::to_string(ope.ignore_case_) + " " + ope.lit_ + ")";
  }
  void visit(CharacterClass &ope) override {
    s += "(cls " + std::to_string(ope.negated_) + " " +
         std::to_string(ope.ignore_case_);
    for (const auto &[lo, hi] : ope.ranges_) {
      s += " " + std::to_string(static_cast<uint32_t>(lo)) + "-" +
           std::to_string(static_cast<uint32_t>(hi));
    }
    s += ')';
  }
  void visit(Character &ope) override {
    s += "(chr " + std::to_string(static_cast<uint32_t>(ope.ch_)) + ")";
  }
  void visit(AnyCharacter &) override { s += "(any)"; }
  void visit(Dictionary &ope) override { opaque(&ope); }
  void visit(User &ope) override { opaque(&ope); }
  void visit(BackReference &ope) override { opaque(&ope); }
  void visit(PrecedenceClimbing &ope) override { opaque(&ope); }
  void visit(Cut &ope) override { opaque(&ope); }

  static std::string get(Ope &ope) {
    OpeSignature vis;
    ope.accept(vis);
    return std::move(vis.s);
  }

private:
  void group(const char *tag, const std::vector<std::shared_ptr<Ope>> &v) {
    s += '(';
    s += tag;
    for (const auto &op : v) {
      s += ' ';
      op->accept(*this);
    }
    s += ')';
  }
  void unary(const char *tag, Ope &inner) {
    s += '(';
    s += tag;
    wrap(inner);
  }
  void wrap(Ope &inner) {
    s += ' ';
    inner.accept(*this);
    s += ')';
  }
  void opaque(const void *p) {
    s += "(opq " + std::to_string(reinterpret_cast<std::uintptr_t>(p)) + ")";
  }
};

// Goes down an operand that can match empty to what makes it: the last
// element of a sequence, the first alternative of a choice that can, and
// into a rule unless it is already on the way. Ends at the innermost named
// rule on the way, where it is referred to.
struct FindEmptyElement : public TraversalVisitor {
  using TraversalVisitor::visit;

  FindEmptyElement(const Definition *rule,
                   std::pair<const char *, std::string> rule_ref)
      : rule_ref(std::move(rule_ref)), rules_on_way_{rule} {}

  void visit(Sequence &ope) override {
    if (!ope.opes_.empty()) { ope.opes_.back()->accept(*this); }
  }
  void visit(PrioritizedChoice &ope) override {
    for (const auto &op : ope.opes_) {
      if (ComputeCanBeEmpty::check(*op)) {
        op->accept(*this);
        return;
      }
    }
  }
  void visit(Repetition &ope) override {
    if (ope.min_ > 0) { ope.ope_->accept(*this); }
  }
  void visit(AndPredicate &) override {}
  void visit(NotPredicate &) override {}
  void visit(Reference &ope) override;

  std::pair<const char *, std::string> rule_ref;

private:
  std::vector<const Definition *> rules_on_way_;
};

// Finds, in a rule and the rules it reaches, each walked once, the first
// unbounded repetition whose operand can match empty. Whether it can is
// worked out for every rule beforehand, so it is the same wherever the
// repetition is reached from.
struct DetectInfiniteLoop : public TraversalVisitor {
  using TraversalVisitor::visit;

  DetectInfiniteLoop(const char *s, const std::string &name)
      : rule_ref_(s, name) {}

  void visit(Sequence &ope) override { walk_until_error(ope.opes_); }
  void visit(PrioritizedChoice &ope) override { walk_until_error(ope.opes_); }
  void visit(Repetition &ope) override {
    if (ope.max_ == std::numeric_limits<size_t>::max() &&
        ComputeCanBeEmpty::check(*ope.ope_)) {
      FindEmptyElement vis(rule_, rule_ref_);
      vis.walk_within(*this);
      ope.ope_->accept(vis);
      has_error = true;
      std::tie(error_s, error_name) = vis.rule_ref;
      return;
    }
    ope.ope_->accept(*this);
  }
  void visit(Reference &ope) override;
  // Walks each rule once, whether reached through a Reference or, made with
  // the combinators, through a WeakHolder.
  void visit(Holder &ope) override {
    if (!walked_.insert(ope.outer_).second) { return; }
    auto save = std::exchange(rule_, ope.outer_);
    ope.ope_->accept(*this);
    rule_ = save;
  }

  bool has_error = false;
  const char *error_s = nullptr;
  std::string error_name;

private:
  void walk_until_error(const std::vector<std::shared_ptr<Ope>> &opes) {
    for (const auto &op : opes) {
      if (has_error) { return; }
      op->accept(*this);
    }
  }

  std::unordered_set<const Definition *> walked_;
  const Definition *rule_ = nullptr;
  // Where the innermost named rule being walked is referred to, also inside
  // the rules made with the combinators that it reaches.
  std::pair<const char *, std::string> rule_ref_;
};

struct ReferenceChecker : public TraversalVisitor {
  using TraversalVisitor::visit;

  ReferenceChecker(const Grammar &grammar,
                   const std::vector<std::string> &params)
      : grammar_(grammar), params_(params) {}

  void visit(Reference &ope) override;

  std::unordered_map<std::string, const char *> error_s;
  std::unordered_map<std::string, std::string> error_message;
  std::unordered_set<std::string> referenced;

private:
  const Grammar &grammar_;
  const std::vector<std::string> &params_;
};

struct LinkReferences : public TraversalVisitor {
  using TraversalVisitor::visit;

  LinkReferences(Grammar &grammar, const std::vector<std::string> &params)
      : grammar_(grammar), params_(params) {}

  void visit(Reference &ope) override;

private:
  Grammar &grammar_;
  const std::vector<std::string> &params_;
};

struct ReplaceParameters : public Ope::Visitor {
  using Ope::Visitor::visit;

  explicit ReplaceParameters(const std::vector<std::shared_ptr<Ope>> &args)
      : args_(args) {}

  // `ope` with the parameters in it replaced by the arguments, or `ope`
  // itself when it names none.
  std::shared_ptr<Ope> resolve(const std::shared_ptr<Ope> &ope) {
    resolved_ = nullptr;
    ope->accept(*this);
    return resolved_ ? std::move(resolved_) : ope;
  }

  void visit(Sequence &ope) override {
    auto opes = resolve_all(ope.opes_);
    if (opes) { resolved_ = std::make_shared<Sequence>(std::move(*opes)); }
  }
  void visit(PrioritizedChoice &ope) override {
    auto opes = resolve_all(ope.opes_);
    if (opes) {
      resolved_ = std::make_shared<PrioritizedChoice>(std::move(*opes), ope);
    }
  }
  void visit(Repetition &ope) override {
    rewrap(ope.ope_, [&](auto o) { return rep(o, ope.min_, ope.max_); });
  }
  void visit(AndPredicate &ope) override { rewrap(ope.ope_, apd); }
  void visit(NotPredicate &ope) override { rewrap(ope.ope_, npd); }
  void visit(CaptureScope &ope) override { rewrap(ope.ope_, csc); }
  void visit(Capture &ope) override {
    rewrap(ope.ope_, [&](auto o) { return cap(o, ope.match_action_); });
  }
  void visit(TokenBoundary &ope) override { rewrap(ope.ope_, tok); }
  void visit(Ignore &ope) override { rewrap(ope.ope_, ign); }
  void visit(Reference &ope) override;
  void visit(Recovery &ope) override { rewrap(ope.ope_, rec); }
  void visit(PrecedenceClimbing &ope) override;

private:
  template <typename Make>
  void rewrap(const std::shared_ptr<Ope> &inner, Make make) {
    auto o = resolve(inner);
    if (o != inner) { resolved_ = make(o); }
  }

  std::optional<std::vector<std::shared_ptr<Ope>>>
  resolve_all(const std::vector<std::shared_ptr<Ope>> &opes) {
    std::optional<std::vector<std::shared_ptr<Ope>>> rebuilt;
    for (size_t i = 0; i < opes.size(); i++) {
      auto o = resolve(opes[i]);
      if (o == opes[i]) { continue; }
      if (!rebuilt) { rebuilt.emplace(opes); }
      (*rebuilt)[i] = std::move(o);
    }
    return rebuilt;
  }

  std::shared_ptr<Ope> resolved_;
  const std::vector<std::shared_ptr<Ope>> &args_;
};

/*
 * First-Set computation
 */
struct ComputeFirstSet : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(Sequence &ope) override {
    for (const auto &op : ope.opes_) {
      if (!add_start_of(*op)) { return; }
    }
    result_.matches_empty_on_other_bytes = true;
  }
  // Where no alternative can start, the first one that matches empty there
  // matches, and the ones after it are not tried. Their bytes are kept all the
  // same: an alternative that is empty only through left recursion fails while
  // its seed is grown.
  void visit(PrioritizedChoice &ope) override {
    std::optional<size_t> noted;
    for (const auto &op : ope.opes_) {
      if (add_start_of(*op) && !noted) {
        result_.matches_empty_on_other_bytes = true;
        noted = result_.failures.size();
      }
    }
    if (noted) { result_.failures.resize(*noted); }
  }
  // A repetition up to no times does not try its operand.
  void visit(Repetition &ope) override {
    if (ope.max_ > 0) { ope.ope_->accept(*this); }
    if (ope.min_ == 0) { result_.matches_empty_on_other_bytes = true; }
  }
  // Where its operand cannot start, the operand fails and this matches
  // empty. An operand that matches empty there makes this fail there
  // instead, which is left to trying it.
  void visit(NotPredicate &ope) override {
    auto first = result_.failures.size();
    if (add_start_of(*ope.ope_)) { result_.any_char = true; }
    for (auto i = first; i < result_.failures.size(); i++) {
      result_.failures[i].listed = false;
    }
    result_.matches_empty_on_other_bytes = true;
  }
  void visit(Dictionary &ope) override {
    for (const auto &[key, info] : ope.trie_.dic_) {
      if (!key.empty()) {
        auto ch = static_cast<unsigned char>(key[0]);
        result_.chars.set(ch);
        if (ope.trie_.ignore_case_) { result_.chars.set(other_case(ch)); }
      }
    }
    result_.failures.emplace_back();
  }
  void visit(LiteralString &ope) override {
    if (ope.lit_.empty()) {
      result_.matches_empty_on_other_bytes = true;
      add_whitespace();
    } else {
      auto ch = static_cast<unsigned char>(ope.lit_[0]);
      result_.chars.set(ch);
      if (ope.ignore_case_) { result_.chars.set(other_case(ch)); }
      result_.failures.push_back({ope.lit_.c_str(), nullptr});
    }
  }
  void visit(CharacterClass &ope) override {
    if (ope.is_ascii_only()) {
      result_.chars |= ope.ascii_bitset();
      result_.failures.emplace_back();
    } else {
      result_.any_char = true;
    }
  }
  void visit(Character &ope) override {
    if (ope.ch_ > 0x7F) {
      result_.any_char = true;
    } else {
      result_.chars.set(static_cast<unsigned char>(ope.ch_));
      result_.failures.emplace_back();
    }
  }
  void visit(AnyCharacter &) override { result_.any_char = true; }
  void visit(TokenBoundary &ope) override {
    ope.ope_->accept(*this);
    if (result_.matches_empty_on_other_bytes) { add_whitespace(); }
  }
  void visit(User &) override { result_.any_char = true; }
  void visit(Reference &ope) override;
  void visit(WeakHolder &ope) override;
  void visit(BackReference &) override { result_.any_char = true; }
  // A recovery gives its label's message wherever it is tried.
  void visit(Recovery &) override { result_.any_char = true; }
  // A cut before the first byte stops the enclosing choice even when the
  // expression then fails, so the next byte cannot tell to skip it.
  void visit(Cut &) override {
    result_.matches_empty_on_other_bytes = true;
    result_.any_char = true;
  }

  // Per-rule cache shared across a SetupFirstSets traversal. Without it,
  // every alternative of every PrioritizedChoice re-walks referenced
  // rules — O(refs^depth) work for grammars with many cross-references.
  // A rule met again in its own walk (left recursion) adds its bytes where
  // it is first walked into, so a result is cached only if every rule met
  // again in its walk is the rule or one walked into below it: one met
  // higher up adds its bytes there, and the result lacks them.
  using FirstSetCache = std::unordered_map<const Definition *, FirstSet>;

  explicit ComputeFirstSet(FirstSetCache &cache) : cache_(cache) {}

  FirstSet result_;

private:
  // Adds where `ope` can start, and what it notes where it cannot. Returns
  // whether it matches empty there.
  bool add_start_of(Ope &ope) {
    auto save = std::exchange(result_.matches_empty_on_other_bytes, false);
    ope.accept(*this);
    return std::exchange(result_.matches_empty_on_other_bytes, save);
  }

  // A literal, a token boundary and a no_whitespace rule skip whitespace
  // after their match. After an empty one, the whitespace can come first,
  // and it is that of the rule the parse starts from, which need not be the
  // grammar's.
  void add_whitespace() { result_.any_char = true; }

  // Puts `rule` on the walk and returns its depth there, unless it is being
  // worked out further up: it adds its bytes there, and what is worked out
  // below it lacks them.
  std::optional<size_t> walk_into(const Definition &rule);

  FirstSetCache &cache_;
  // The rules being worked out, each at its depth in the walk.
  std::vector<const Definition *> refs_;
  // The least depth of a rule met again since the innermost walk into a
  // referenced rule began.
  size_t least_depth_met_ = std::numeric_limits<size_t>::max();
};

struct SetupFirstSets : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(PrioritizedChoice &ope) override {
    std::vector<FirstSet> first_sets;
    first_sets.reserve(ope.opes_.size());
    for (const auto &op : ope.opes_) {
      ComputeFirstSet cfs(first_set_cache_);
      op->accept(cfs);
      first_sets.push_back(std::move(cfs.result_));
    }
    ope.set_first_sets(std::move(first_sets));
    for (const auto &op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override {
    ope.ope_->accept(*this);
    // ISpan optimization: detect Repetition + ASCII CharacterClass
    auto cc = dynamic_cast<CharacterClass *>(ope.ope_.get());
    if (cc && cc->is_ascii_only()) { ope.span_bitset_ = &cc->ascii_bitset(); }
  }
  void visit(Reference &ope) override;
  void visit(Holder &ope) override;

private:
  ComputeFirstSet::FirstSetCache first_set_cache_;
  std::unordered_set<const Definition *> visited_rules_;
};

/*
 * Keywords
 */
inline constexpr const char *WHITESPACE_DEFINITION_NAME = "%whitespace";
inline constexpr const char *WORD_DEFINITION_NAME = "%word";
inline constexpr const char *RECOVER_DEFINITION_NAME = "%recover";

/*
 * Definition
 */
struct CollectRuleRefs;

class Definition {
public:
  struct Result {
    bool ret;
    bool recovered;
    size_t len;
    ErrorInfo error_info;
  };

  Definition() : holder_(std::make_shared<Holder>(this)) {}

  // A rule is referred to by address, so it cannot be copied: a copy would
  // share the holder and leave it pointing at whichever of the two goes away
  // first. Take a reference instead (`auto &rule = parser["A"]`). Moving one
  // hands the holder over, for a rule built and returned by value.
  Definition(const Definition &) = delete;
  Definition &operator=(const Definition &) = delete;
  Definition &operator=(Definition &&) = delete;

  Definition(Definition &&rhs)
      : name(std::move(rhs.name)), holder_(std::move(rhs.holder_)) {
    holder_->outer_ = this;
  }

  Definition(const std::shared_ptr<Ope> &ope)
      : holder_(std::make_shared<Holder>(this)) {
    *this <= ope;
  }

  operator std::shared_ptr<Ope>() {
    return std::make_shared<WeakHolder>(holder_);
  }

  Definition &operator<=(const std::shared_ptr<Ope> &ope) {
    holder_->ope_ = ope;
    return *this;
  }

  Result parse(const char *s, size_t n, const char *path = nullptr,
               Log log = nullptr,
               ErrorReporter error_reporter = nullptr) const {
    SemanticValues vs;
    std::any dt;
    return parse_core(s, n, vs, dt, path, log, error_reporter);
  }

  Result parse(const char *s, const char *path = nullptr, Log log = nullptr,
               ErrorReporter error_reporter = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, path, log, error_reporter);
  }

  Result parse(const char *s, size_t n, std::any &dt,
               const char *path = nullptr, Log log = nullptr,
               ErrorReporter error_reporter = nullptr) const {
    SemanticValues vs;
    return parse_core(s, n, vs, dt, path, log, error_reporter);
  }

  Result parse(const char *s, std::any &dt, const char *path = nullptr,
               Log log = nullptr,
               ErrorReporter error_reporter = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, dt, path, log, error_reporter);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, T &val,
                             const char *path = nullptr, Log log = nullptr,
                             ErrorReporter error_reporter = nullptr) const {
    SemanticValues vs;
    std::any dt;
    auto r = parse_core(s, n, vs, dt, path, log, error_reporter);
    if (r.ret && !vs.empty() && vs.front().has_value()) {
      val = std::any_cast<T>(vs[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, T &val, const char *path = nullptr,
                             Log log = nullptr,
                             ErrorReporter error_reporter = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, val, path, log, error_reporter);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, std::any &dt, T &val,
                             const char *path = nullptr, Log log = nullptr,
                             ErrorReporter error_reporter = nullptr) const {
    SemanticValues vs;
    auto r = parse_core(s, n, vs, dt, path, log, error_reporter);
    if (r.ret && !vs.empty() && vs.front().has_value()) {
      val = std::any_cast<T>(vs[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, std::any &dt, T &val,
                             const char *path = nullptr, Log log = nullptr,
                             ErrorReporter error_reporter = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, dt, val, path, log, error_reporter);
  }

#if defined(__cpp_lib_char8_t)
  Result parse(const char8_t *s, size_t n, const char *path = nullptr,
               Log log = nullptr) const {
    return parse(u8(s), n, path, log);
  }

  Result parse(const char8_t *s, const char *path = nullptr,
               Log log = nullptr) const {
    return parse(u8(s), path, log);
  }

  Result parse(const char8_t *s, size_t n, std::any &dt,
               const char *path = nullptr, Log log = nullptr) const {
    return parse(u8(s), n, dt, path, log);
  }

  Result parse(const char8_t *s, std::any &dt, const char *path = nullptr,
               Log log = nullptr) const {
    return parse(u8(s), dt, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char8_t *s, size_t n, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    return parse_and_get_value(u8(s), n, val, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char8_t *s, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    return parse_and_get_value(u8(s), val, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char8_t *s, size_t n, std::any &dt, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    return parse_and_get_value(u8(s), n, dt, val, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char8_t *s, std::any &dt, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    return parse_and_get_value(u8(s), dt, val, path, log);
  }
#endif

  void operator=(Action a) { action = a; }

  template <typename T> Definition &operator,(T fn) {
    operator=(fn);
    return *this;
  }

  Definition &operator~() {
    ignoreSemanticValue = true;
    return *this;
  }

  void accept(Ope::Visitor &v) { holder_->accept(v); }

  std::shared_ptr<Ope> get_core_operator() const { return holder_->ope_; }

  bool is_token() const {
    std::call_once(is_token_init_, [this]() {
      is_token_ = TokenChecker::is_token(*get_core_operator());
    });
    return is_token_;
  }

  const std::string &node_name() const {
    return ast_name.empty() ? name : ast_name;
  }

  unsigned int tag() const {
    std::call_once(tag_init_, [this]() { tag_ = str2tag(name); });
    return tag_;
  }

  std::string name;
  const char *s_ = nullptr;
  std::pair<size_t, size_t> line_ = {1, 1};

  Predicate predicate;

  size_t id = 0;
  Action action;
  std::function<void(const Context &c, const char *s, size_t n, std::any &dt)>
      enter;
  std::function<void(const Context &c, const char *s, size_t n, size_t matchlen,
                     std::any &value, std::any &dt)>
      leave;
  bool ignoreSemanticValue = false;
  std::shared_ptr<Ope> whitespaceOpe;
  std::shared_ptr<Ope> wordOpe;
  bool enablePackratParsing = false;
  bool is_macro = false;
  std::vector<std::string> params;
  bool disable_action = false;
  bool is_left_recursive = false;
  bool can_be_empty = false;
  // The bytes a match of this rule can start with: any, unless it fails on
  // other bytes and they are known (set up with the first sets).
  std::bitset<256> start_bytes = std::bitset<256>().set();
  // The first set of the rule's operator.
  FirstSet first_set;

  TracerEnter tracer_enter;
  TracerLeave tracer_leave;
  bool verbose_trace = false;
  TracerStartOrEnd tracer_start;
  TracerStartOrEnd tracer_end;

  std::string error_message;
  bool no_ast_opt = false;
  bool no_whitespace = false; // Disable %whitespace skipping inside this rule
                              // (like a token boundary, without capturing)
  std::string ast_name; // When non-empty, AST nodes produced by this rule carry
                        // this name/tag instead of the rule's own name
  // Fixed by add_ast_action when it attaches the AST action, which gives its
  // nodes this name and tag.
  std::string ast_node_name;
  unsigned int ast_node_tag = 0;

  bool eoi_check = true;

  // The deepest that rule matches may nest in a parse from this rule.
  size_t max_depth = std::numeric_limits<size_t>::max();

  // Per-rule packrat stats (optional, for profiling)
  mutable bool collect_packrat_stats = false;
  mutable std::vector<Context::PackratStats> packrat_stats_;

private:
  friend class Context;
  friend class Reference;
  friend class ParserGenerator;

  const StartRuleAnalysis &analysis() const {
    std::call_once(analysis_init_, [&]() { analyze(); });
    return analysis_;
  }

  void analyze() const;
  void number_new_rules() const;
  void decide_parsed_only_where_muted(std::vector<RuleUse> &uses) const;
  void select_packrat_rules() const;
  std::shared_ptr<const RuleUses> rule_uses(bool builds_everything) const;
  bool update_rule_use_inputs(bool builds_everything) const;
  std::vector<RuleUse> analyze_rule_uses() const;
  void decide_value_use(const std::vector<CollectRuleRefs> &refs,
                        const CollectRuleRefs &whitespace,
                        std::vector<RuleUse> &uses) const;
  void decide_skippable(std::vector<RuleUse> &uses) const;
  void decide_must_be_tried(RuleUses &uses) const;

  // Whether a rule in `rules` is flagged. One that this start rule does not
  // index is taken to be.
  template <typename Rules>
  bool any_flagged(const std::vector<bool> &flag, const Rules &rules) const {
    return std::any_of(rules.begin(), rules.end(), [&](const auto *r) {
      return !analysis_.indexes(*r) || flag[r->id];
    });
  }

  // A flag spreads to the rules that refer to a flagged rule.
  template <typename RulesOf>
  void spread(std::vector<bool> &flag, RulesOf rules_of) const {
    for (auto again = true; again;) {
      again = false;
      for (auto *rule : analysis_.rules_by_id) {
        if (rule && !flag[rule->id] && any_flagged(flag, rules_of(rule->id))) {
          flag[rule->id] = again = true;
        }
      }
    }
  }

  Result parse_core(const char *s, size_t n, SemanticValues &vs, std::any &dt,
                    const char *path, Log log,
                    ErrorReporter error_reporter = nullptr) const {
    const auto &analysis = this->analysis();

    std::shared_ptr<Ope> ope = holder_;

    std::any trace_data;
    if (tracer_start) { tracer_start(trace_data); }
    auto se = scope_exit([&]() {
      if (tracer_end) { tracer_end(trace_data); }
    });

    if (enablePackratParsing) { select_packrat_rules(); }

    Context c(path, s, n, &analysis, whitespaceOpe, wordOpe,
              enablePackratParsing, tracer_enter, tracer_leave, trace_data,
              verbose_trace, log, error_reporter);

    if (collect_packrat_stats) {
      packrat_stats_.resize(analysis.rules_by_id.size());
      c.packrat_stats = &packrat_stats_;
    }

    c.builds_everything = c.has_tracer || analysis.has_user;
    auto uses = rule_uses(c.builds_everything);
    c.rule_use = uses->by_id.data();

    c.max_depth = max_depth;
    c.skips_rules = !c.has_tracer && takes_shortcuts();
    c.notices_skipping = c.reports_errors || c.limits_depth();
    c.checks_alternatives = c.notices_skipping && uses->some_must_be_tried;

    auto result = [&](bool ret, size_t len) {
      c.error_info.resolve_expected_tokens();
      return Result{ret, c.recovered, len, c.error_info};
    };

    try {
      size_t i = 0;

      if (whitespaceOpe) {
        Context::TraceMute mute(c);

        auto len = whitespaceOpe->parse(s, n, vs, c, dt);
        if (fail(len)) { return result(false, i); }

        i = len;
      }

      auto len = ope->parse(s + i, n - i, vs, c, dt);
      auto ret = success(len);
      if (ret) {
        i += len;
        if (eoi_check) {
          if (i < n) {
            if (c.error_info.error_pos < s + i) {
              c.error_info.set_message(s + i, "expected end of input");
            }
            ret = false;
          }
        }
      }
      return result(ret, i);
    } catch (const NestingTooDeep &e) {
      c.error_info.set_nesting_too_deep(e.pos, e.rule->name, max_depth);
      return result(false, 0);
    }
  }

  std::shared_ptr<Holder> holder_;
  mutable std::once_flag is_token_init_;
  mutable bool is_token_ = false;
  mutable std::once_flag tag_init_;
  mutable unsigned int tag_ = 0;
  mutable bool has_id_ = false;
  mutable std::once_flag analysis_init_;
  mutable std::once_flag packrat_rules_init_;
  mutable StartRuleAnalysis analysis_;

  // What rule_uses_ was made from: what it is made for, and of each rule
  // the type of its AST node and the callbacks it has.
  struct RuleUseInputs {
    bool builds_everything = false;
    bool packrat = false;
    std::vector<std::pair<const std::type_info *, uint8_t>> rules;
  };
  mutable std::mutex rule_uses_mutex_;
  mutable RuleUseInputs rule_use_inputs_;
  mutable std::shared_ptr<const RuleUses> rule_uses_;
};

inline bool StartRuleAnalysis::indexes(const Definition &rule) const {
  return rule.id < rules_by_id.size() && rules_by_id[rule.id] == &rule;
}

/*
 * Implementations
 */

inline bool starts_word(const Context &c, const char *s, size_t n) {
  SemanticValues dummy_vs;
  auto dummy_c = Context::detached(c);
  std::any dummy_dt;
  return success(c.wordOpe->parse(s, n, dummy_vs, dummy_c, dummy_dt));
}

inline size_t parse_literal(const char *s, size_t n, SemanticValues &vs,
                            Context &c, std::any &dt, const std::string &lit,
                            std::once_flag &init_is_word, bool &is_word,
                            bool ignore_case, const std::string &lower_lit,
                            bool copy_lit) {
  size_t i = 0;
  for (; i < lit.size(); i++) {
    if (i >= n ||
        (ignore_case ? to_lower(s[i]) != lower_lit[i] : s[i] != lit[i])) {
      c.set_error_pos(s, lit.data(), copy_lit);
      return kFail;
    }
  }

  // Word check
  if (c.wordOpe) {
    std::call_once(init_is_word,
                   [&]() { is_word = starts_word(c, lit.data(), lit.size()); });

    if (is_word && starts_word(c, s + i, n - i)) {
      c.set_error_pos(s, lit.data(), copy_lit);
      return kFail;
    }
  }

  // Skip whitespace
  auto wl = c.skip_whitespace(s + i, n - i, vs, dt);
  if (fail(wl)) { return wl; }
  i += wl;

  return i;
}

inline std::pair<size_t, size_t> SemanticValues::line_info() const {
  assert(c_);
  return c_->line_info(sv_.data());
}

// A failure is listed under the outermost token rule it is in, or else its
// own rule.
inline void ErrorInfo::insert_failure(const char *literal,
                                      const Definition *rule,
                                      const Definition *token_rule) {
  const auto *error_rule = token_rule ? token_rule : rule;
  if (literal || error_rule) {
    expected_tokens.emplace_back(literal, error_rule);
  }
}

// Trying a rule notes its own start failures inside it, under itself if it is
// the outermost token rule.
inline void ErrorInfo::insert_start_failure(const StartFailure &failure,
                                            const Definition *rule,
                                            const Definition *token_rule,
                                            Expanded &expanded) {
  if (!failure.listed) { return; }
  if (!failure.rule) {
    insert_failure(failure.literal, rule, token_rule);
    return;
  }
  const auto *inner = failure.rule;
  auto inner_token_rule = !token_rule && inner->is_token() ? inner : token_rule;
  auto key = std::pair(inner, inner_token_rule);
  if (std::find(expanded.begin(), expanded.end(), key) != expanded.end()) {
    return;
  }
  expanded.push_back(key);
  for (const auto &f : inner->first_set.failures) {
    insert_start_failure(f, inner, inner_token_rule, expanded);
  }
}

// Out of line, so that note() is small enough to be inlined.
CPPPEGLIB_NOINLINE inline void ErrorInfo::remove_repeats() {
  removed_ += drop_repeats(events_, boundary_,
                           [](const ExpectedEvent &e) { return e.hash(); });
  remove_repeats_at_ = std::max(max_events_, 2 * (events_.size() - boundary_));
}

// The events of a memoized parse are named by the outermost token rule in
// it, and the parse around it names them by its own, if it has one: they are
// read under the outermost name given from outside.
inline void ErrorInfo::resolve_expected_tokens() {
  if (!has_message()) { clear_message(); }

  // The spans of the memoized parses, nested. One still in progress spans to
  // the end, and of two with the same span the one done later is around.
  struct Span : Record {
    std::vector<size_t> inner;
  };
  std::vector<Span> spans;
  for (const auto &f : frames_) {
    if (auto span = span_of(f); span.begin < span.end) {
      spans.push_back({span, {}});
    }
  }
  std::vector<size_t> span_of_record(records_.size());
  for (auto i = records_.size(); i-- > 0;) {
    span_of_record[i] = spans.size();
    spans.push_back({records_[i], {}});
  }
  std::vector<size_t> order;
  for (size_t i = 0; i < spans.size(); i++) {
    order.push_back(i);
  }
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return spans[a].begin != spans[b].begin ? spans[a].begin < spans[b].begin
                                            : spans[a].end > spans[b].end;
  });
  std::vector<size_t> outermost, around;
  for (auto i : order) {
    while (!around.empty() && spans[around.back()].end <= spans[i].begin) {
      around.pop_back();
    }
    (around.empty() ? outermost : spans[around.back()].inner).push_back(i);
    around.push_back(i);
  }

  Expanded expanded;
  // A span read again under the same token rule adds nothing.
  std::vector<std::vector<const Definition *>> read_under(spans.size());
  auto read = [&](auto &self, const std::vector<size_t> &inner, size_t begin,
                  size_t end, const Definition *token_rule) -> void {
    auto read_span = [&](size_t i, const Definition *t) {
      auto &done = read_under[i];
      if (std::find(done.begin(), done.end(), t) != done.end()) { return; }
      done.push_back(t);
      self(self, spans[i].inner, spans[i].begin, spans[i].end, t);
    };
    auto next = inner.begin();
    for (auto i = begin; i < end;) {
      if (next != inner.end() && spans[*next].begin == i) {
        auto inner_span = *next++;
        read_span(inner_span,
                  token_rule ? token_rule : spans[inner_span].token_rule);
        i = spans[inner_span].end;
        continue;
      }
      const auto &e = events_[i++];
      auto t = token_rule ? token_rule : e.token_rule;
      switch (e.kind) {
      case Kind::failure:
        insert_start_failure(e.failure, e.rule, t, expanded);
        break;
      case Kind::skipped:
        for (auto id = e.begin; id < e.end; id++) {
          for (const auto &f : e.choice->first_sets()[id].failures) {
            insert_start_failure(f, e.rule, t, expanded);
          }
        }
        break;
      case Kind::replay: read_span(span_of_record[e.record], t); break;
      }
    }
  };
  // What an earlier resolve left has no repeats.
  auto resolved = expected_tokens.size();
  read(read, outermost, 0, events_.size(), nullptr);
  if (expected_tokens.size() != resolved) {
    drop_repeats(expected_tokens, 0, [](const auto &token) {
      return mix_index(reinterpret_cast<uintptr_t>(token.first) * 31 +
                       reinterpret_cast<uintptr_t>(token.second));
    });
  }
  next_generation();
}

inline void ErrorInfo::output_log(const Log &log, const ErrorReporter &reporter,
                                  const char *s, size_t n,
                                  const std::string &recovery_label) {
  resolve_expected_tokens();
  auto pos = message_pos ? message_pos : error_pos;
  if (!pos || pos <= last_output_pos) { return; }
  last_output_pos = pos;

  auto line = line_info(s, pos);
  ErrorReport report;
  report.line = line.first;
  report.col = line.second;
  report.position = static_cast<size_t>(pos - s);

  auto unexpected_token = heuristic_error_token(s, n, pos);
  std::string msg;
  if (message_pos) {
    if (!unexpected_token.empty()) {
      msg = replace_all(message, "%t", unexpected_token);

      auto unexpected_char = unexpected_token.substr(
          0, codepoint_or_byte_length(unexpected_token.data(),
                                      unexpected_token.size()));

      msg = replace_all(msg, "%c", unexpected_char);
    } else {
      msg = message;
    }
    report.unexpected_token = unexpected_token;
    report.message = msg;
    report.label = label;
  } else if (expected_tokens.empty()) {
    msg = "syntax error.";
  } else {
    msg = "syntax error";

    if (!unexpected_token.empty()) {
      msg += ", unexpected '";
      msg += unexpected_token;
      msg += "'";
      report.unexpected_token = unexpected_token;
    }

    // A literal is listed by its text, which can come from more than one
    // place in the grammar. Nothing is listed under a rule that starts with
    // '_'.
    std::vector<std::pair<std::string_view, const Definition *>> listed;
    listed.reserve(expected_tokens.size());
    for (const auto &[literal, rule] : expected_tokens) {
      if (rule && rule->name[0] == '_') { continue; }
      listed.emplace_back(literal ? literal : "", literal ? nullptr : rule);
    }
    drop_repeats(listed, 0, [](const auto &item) {
      return std::hash<std::string_view>()(item.first) ^
             mix_index(reinterpret_cast<uintptr_t>(item.second));
    });
    for (const auto &[text, rule] : listed) {
      msg += report.expected_literals.empty() && report.expected_rules.empty()
                 ? ", expecting "
                 : ", ";
      if (rule) {
        msg += "<" + rule->name + ">";
        report.expected_rules.emplace_back(rule->name);
      } else {
        msg += "'";
        msg += text;
        msg += "'";
        report.expected_literals.emplace_back(text);
      }
    }
    msg += ".";
    if (!report.expected_rules.empty()) {
      report.label = report.expected_rules.front();
    }
  }

  if (!recovery_label.empty()) { report.label = recovery_label; }
  if (reporter) { reporter(report); }
  if (log) { log(line.first, line.second, msg, report.label); }
}

inline size_t Context::skip_whitespace(const char *a_s, size_t n,
                                       SemanticValues &vs, std::any &dt) {
  if (skips_no_whitespace || !whitespaceOpe) { return 0; }
  TraceMute mute(*this);
  return whitespaceOpe->parse(a_s, n, vs, *this, dt);
}

inline size_t Context::parse_without_whitespace(const Ope &ope, const char *a_s,
                                                size_t n, SemanticValues &vs,
                                                std::any &dt) {
  auto saved = std::exchange(skips_no_whitespace, whitespaceOpe != nullptr);
  auto se = scope_exit([&]() { skips_no_whitespace = saved; });
  return ope.parse(a_s, n, vs, *this, dt);
}

// Reads is_token_ as it is: the start rule's analysis has asked every rule
// it reaches whether it is a token, so that this need not on every push.
inline void Context::push_rule(Definition *rule) {
  if (outer_token_rule == static_cast<size_t>(-1) && rule->is_token_) {
    outer_token_rule = rule_stack.size();
  }
  rule_stack.push_back(rule);
}

inline void Context::trace_enter(const Ope &ope, const char *a_s, size_t n,
                                 const SemanticValues &vs, std::any &dt) {
  trace_ids.push_back(next_trace_id++);
  tracer_enter(ope, a_s, n, vs, *this, dt, trace_data);
}

inline void Context::trace_leave(const Ope &ope, const char *a_s, size_t n,
                                 const SemanticValues &vs, std::any &dt,
                                 size_t len) {
  tracer_leave(ope, a_s, n, vs, *this, dt, len, trace_data);
  trace_ids.pop_back();
}

inline bool Context::is_traceable(const Ope &ope) const {
  if (has_tracer) {
    if (ignore_trace_state) { return false; }
    return !dynamic_cast<const peg::Reference *>(&ope);
  }
  return false;
}

inline size_t Ope::parse(const char *s, size_t n, SemanticValues &vs,
                         Context &c, std::any &dt) const {
  if (c.has_tracer) { return parse_traced(s, n, vs, c, dt); }
  return parse_core(s, n, vs, c, dt);
}

// Out of line, so that Ope::parse is small enough to be inlined into its
// callers.
CPPPEGLIB_NOINLINE inline size_t Ope::parse_traced(const char *s, size_t n,
                                                   SemanticValues &vs,
                                                   Context &c,
                                                   std::any &dt) const {
  if (!c.is_traceable(*this)) { return parse_core(s, n, vs, c, dt); }
  c.trace_enter(*this, s, n, vs, dt);
  auto len = parse_core(s, n, vs, c, dt);
  c.trace_leave(*this, s, n, vs, dt, len);
  return len;
}

inline size_t Dictionary::parse_core(const char *s, size_t n,
                                     SemanticValues &vs, Context &c,
                                     std::any &dt) const {
  size_t id;
  auto i = trie_.match(s, n, id);

  if (i == 0) {
    c.set_error_pos(s);
    return kFail;
  }

  vs.choice_count_ = trie_.items_count();
  vs.choice_ = id;

  // Word check
  if (c.wordOpe && starts_word(c, s + i, n - i)) {
    c.set_error_pos(s);
    return kFail;
  }

  // Skip whitespace
  auto wl = c.skip_whitespace(s + i, n - i, vs, dt);
  if (fail(wl)) { return wl; }
  i += wl;

  return i;
}

inline size_t LiteralString::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  return parse_literal(s, n, vs, c, dt, lit_, init_is_word_, is_word_,
                       ignore_case_, lower_lit_, false);
}

inline size_t TokenBoundary::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  Context::TraceMute mute(c);

  auto len = c.parse_without_whitespace(*ope_, s, n, vs, dt);

  if (success(len)) {
    if (vs.is_read_ && !vs.holds_rule_tokens_) {
      vs.tokens.emplace_back(std::string_view(s, len));
    }

    auto wl = c.skip_whitespace(s + len, n - len, vs, dt);
    if (fail(wl)) { return wl; }
    len += wl;
  }
  return len;
}

// Resolve `%{name}` placeholders in a custom error message against the
// named captures recorded so far ($name<...>). Unknown names resolve to an
// empty string. `%t` / `%c` are resolved later, at log-output time.
inline std::string resolve_capture_placeholders(const std::string &msg,
                                                const Context &c) {
  auto pos = msg.find("%{");
  if (pos == std::string::npos) { return msg; }

  std::string r;
  size_t i = 0;
  while (pos != std::string::npos) {
    auto end = msg.find('}', pos + 2);
    if (end == std::string::npos) { break; }
    r.append(msg, i, pos - i);
    auto name = std::string_view(msg).substr(pos + 2, end - (pos + 2));
    for (auto it = c.capture_entries.rbegin(); it != c.capture_entries.rend();
         ++it) {
      if (it->first == name) {
        // The captured span can include whitespace skipped after a token
        // boundary; trim it for display.
        auto v = std::string_view(it->second);
        while (!v.empty() &&
               std::isspace(static_cast<unsigned char>(v.back()))) {
          v.remove_suffix(1);
        }
        while (!v.empty() &&
               std::isspace(static_cast<unsigned char>(v.front()))) {
          v.remove_prefix(1);
        }
        r += v;
        break;
      }
    }
    i = end + 1;
    pos = msg.find("%{", i);
  }
  r.append(msg, i, msg.size() - i);
  return r;
}

// Parses the rule's operator into the given scope, maintaining the rule
// stack and honouring no_whitespace.
inline size_t Holder::parse_ope_body(const char *s, size_t n,
                                     SemanticValues &vs, Context &c,
                                     std::any &dt) const {
  if (c.needs_rule_stack) { c.push_rule(outer_); }

  size_t len;
  if (outer_->no_whitespace) {
    len = c.parse_without_whitespace(*ope_, s, n, vs, dt);
    if (success(len)) {
      auto wl = c.skip_whitespace(s + len, n - len, vs, dt);
      if (fail(wl)) {
        len = wl;
      } else {
        len += wl;
      }
    }
  } else {
    len = ope_->parse(s, n, vs, c, dt);
  }

  if (c.needs_rule_stack) { c.pop_rule(); }
  return len;
}

CPPPEGLIB_NOINLINE inline void Context::note_failure(const char *a_s,
                                                     StartFailure failure,
                                                     bool copy_literal) {
  if (error_info.advance_to(a_s)) {
    error_info.add(failure, current_rule(), token_rule(), copy_literal);
  }
}

inline bool Context::must_be_tried(const FirstSet &fs) const {
  return std::any_of(
      fs.failures.begin(), fs.failures.end(), [&](const auto &f) {
        return f.rule &&
               (!has_id(*f.rule) || rule_use[f.rule->id].must_be_tried);
      });
}

inline size_t Holder::parse_core(const char *s, size_t n, SemanticValues &vs,
                                 Context &c, std::any &dt) const {
  if (!ope_) {
    throw std::logic_error("Uninitialized definition ope was used...");
  }

  auto nesting = c.nest(s, *outer_);

  // Macro reference. A left-recursive macro cannot take this path: it needs
  // its seed grown (Context::reuse_or_grow), which in turn needs its own
  // semantic value scope to memoise. Such a macro forms a scope like a plain
  // rule does.
  // An ignored one is parsed into a scope that is thrown away, as `~` does.
  if (outer_->is_macro && !outer_->is_left_recursive) {
    if (!outer_->ignoreSemanticValue) {
      return parse_ope_body(s, n, vs, c, dt);
    }
    return c.parse_thrown_away([&](SemanticValues &chvs) {
      return parse_ope_body(s, n, chvs, c, dt);
    });
  }

  auto has_id = c.has_id(*outer_);
  const auto use = has_id ? c.rule_use[outer_->id] : RuleUse();

  Context::RuleMatch match;

  // A match that nothing observes, and whose value and token nothing reads,
  // is only recognized. A value that is always empty is delivered as such.
  if (!vs.holds_rule_tokens_ &&
      (use.recognized == RuleUse::Recognized::always ||
       (use.recognized == RuleUse::Recognized::where_unread &&
        !vs.holds_rule_values_))) {
    // A left-recursive rule is observed (Definition::decide_value_use): its
    // seed is grown in reuse_or_grow.
    assert(!outer_->is_left_recursive);
    c.reuse_or_parse(s, *outer_, has_id, false, match,
                     [&](Context::RuleMatch &m) {
                       m.len = parse_ope_body(s, n, c.unread_scope, c, dt);
                     });
    if (success(match.len) && vs.holds_rule_values_ &&
        !outer_->ignoreSemanticValue) {
      // A value that might not be empty is read nowhere.
      assert(use.value_always_empty);
      vs.emplace_back();
      vs.tags.emplace_back(outer_->tag());
    }
    return match.len;
  }

  // Builds a match: invokes enter/leave callbacks, parses the rule's
  // operator, handles actions/predicates/errors, and calls reduce.
  // Writes into m (its value and token only on success). A left-recursive
  // rule runs it once more for every growth of its seed.
  auto do_parse = [&](Context::RuleMatch &m) {
    if (outer_->enter) { outer_->enter(c, s, n, dt); }
    auto &chvs = c.push_semantic_values_scope();
    auto se = scope_exit([&]() { c.pop_semantic_values_scope(); });
    chvs.holds_rule_values_ =
        use.child_values == RuleUse::ChildValues::read ||
        (use.child_values == RuleUse::ChildValues::passed_up &&
         vs.holds_rule_values_);

    const auto mark = c.error_info.mark();
    m.len = parse_ope_body(s, n, chvs, c, dt);

    if (success(m.len)) {
      chvs.sv_ = std::string_view(s, m.len);
      chvs.name_ = &outer_->name;

      auto ope_ptr = ope_.get();
      if (ope_ptr->is_token_boundary) {
        ope_ptr = static_cast<const peg::TokenBoundary *>(ope_ptr)->ope_.get();
      }
      if (!ope_ptr->is_choice_like) {
        chvs.choice_count_ = 0;
        chvs.choice_ = 0;
      }
      m.token = chvs.token();

      if (outer_->predicate) {
        std::string msg;
        std::any predicate_data;
        if (!outer_->predicate(chvs, dt, msg, predicate_data)) {
          if (!msg.empty()) {
            c.give_message(
                s, mark, [&] { return std::move(msg); }, outer_->name);
          }
          m.len = kFail;
        } else if (!c.recovered) {
          m.val = reduce(chvs, dt, predicate_data);
        }
      } else if (!c.recovered) {
        std::any predicate_data;
        m.val = reduce(chvs, dt, predicate_data);
      }
    } else {
      if (!outer_->error_message.empty()) {
        c.give_message(
            s, mark,
            [&] {
              return resolve_capture_placeholders(outer_->error_message, c);
            },
            outer_->name);
      }
    }

    // Not from the scope_exit: an exception leaves no result to report.
    if (outer_->leave) { outer_->leave(c, s, n, m.len, m.val, dt); }
  };

  if (outer_->is_left_recursive) {
    c.reuse_or_grow(s, *this, match, do_parse);
  } else {
    c.reuse_or_parse(s, *outer_, has_id, vs.holds_rule_tokens_, match,
                     do_parse);
  }

  if (success(match.len)) {
    if (!outer_->ignoreSemanticValue && vs.holds_rule_values_) {
      vs.emplace_back(std::move(match.val));
      vs.tags.emplace_back(outer_->tag());
    }
    if (vs.holds_rule_tokens_) { vs.tokens.push_back(match.token); }
  }

  return match.len;
}

inline std::any Holder::reduce(SemanticValues &vs, std::any &dt,
                               const std::any &predicate_data) const {
  if (outer_->action && !outer_->disable_action) {
    auto val = outer_->action(vs, dt, predicate_data);
    // Release the values now instead of when this scope is next used: the
    // AST node an action just returned is then held only by the caller, which
    // lets a collapsing parent take it over in place.
    vs.clear();
    vs.tags.clear();
    return val;
  } else if (vs.empty()) {
    return std::any();
  } else {
    return std::move(vs.front());
  }
}

inline const std::string &Holder::name() const { return outer_->name; }

inline const std::string &Holder::trace_name() const {
  std::call_once(trace_name_init_,
                 [this]() { trace_name_ = "[" + outer_->name + "]"; });
  return trace_name_;
}

// Key a macro instantiation by what each argument denotes rather than by the
// node that spells it: `M(N)` written at two call sites builds two Reference
// nodes for the same rule N, and those are the same instantiation. Any other
// argument, a macro call among them, is the node itself.
inline std::vector<const void *>
macro_inst_key(const Definition *def,
               const std::vector<std::shared_ptr<Ope>> &args) {
  std::vector<const void *> key;
  key.reserve(args.size() + 1);
  key.push_back(def);
  for (const auto &arg : args) {
    auto ref = dynamic_cast<Reference *>(arg.get());
    key.push_back(ref && ref->rule_ && !ref->is_macro_
                      ? static_cast<const void *>(ref->rule_)
                      : static_cast<const void *>(arg.get()));
  }
  return key;
}

// A parameter passes its caller's argument on as it is, so a call that
// passes a parameter on finds its own instance again.
inline Context::Instance &Context::instance(const Reference &call) {
  auto &inst = instance_of_call[&call];
  if (!inst) {
    inst = &instance_by_key[macro_inst_key(call.rule_, call.args_)];
    if (!inst->body) {
      auto made = std::make_shared<Holder>(call.rule_);
      made->ope_ = ReplaceParameters(call.args_)
                       .resolve(call.rule_->get_core_operator());
      instances.push_back(made);
      inst->body = made.get();
    }
  }
  return *inst;
}

inline size_t Reference::parse_core(const char *s, size_t n, SemanticValues &vs,
                                    Context &c, std::any &dt) const {
  // ignore_trace_state is only ever read when a tracer is attached, so the
  // save/flip/restore dance is skipped entirely without one.
  if (c.has_tracer) {
    auto save_ignore_trace_state = c.ignore_trace_state;
    if (rule_ && rule_->ignoreSemanticValue) {
      c.ignore_trace_state = !c.verbose_trace;
    }
    auto se =
        scope_exit([&]() { c.ignore_trace_state = save_ignore_trace_state; });
    return parse_dispatch(s, n, vs, c, dt);
  }
  return parse_dispatch(s, n, vs, c, dt);
}

inline size_t Reference::parse_dispatch(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  // An instance has its parameters replaced, and only instances are parsed.
  if (!rule_) {
    throw std::logic_error("A macro parameter was parsed outside its macro...");
  }

  if (rule_->is_macro) {
    auto &inst = c.instance(*this);
    if (rule_->is_left_recursive) { return inst.body->parse(s, n, vs, c, dt); }
    // Left recursion that detection missed enters it again where it is being
    // parsed. It fails there, as a rule does (Context::reuse_or_parse).
    auto &active = inst.active_pos[c.skips_no_whitespace];
    if (c.enters_again(active, s)) { return kFail; }
    // No scope_exit: an exception abandons the Context.
    auto save = std::exchange(active, s);
    auto len = inst.body->parse(s, n, vs, c, dt);
    active = save;
    return len;
  }

  // A rule that cannot start with the next byte fails without being entered,
  // as a choice skips an alternative that cannot. Where that could be
  // noticed, it notes what entering it would, unless it must be tried.
  if (c.skips_rules && n > 0 &&
      !rule_->start_bytes.test(static_cast<unsigned char>(*s)) &&
      c.has_id(*rule_) && c.rule_use[rule_->id].skippable) {
    if (!c.notices_skipping) { return kFail; }
    if (!c.rule_use[rule_->id].must_be_tried) {
      c.note_untried(*rule_, s);
      return kFail;
    }
  }
  return rule_->holder_->parse(s, n, vs, c, dt);
}

inline size_t BackReference::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  for (auto it = c.capture_entries.rbegin(); it != c.capture_entries.rend();
       ++it) {
    if (it->first == name_) {
      const auto &lit = it->second;
      std::once_flag init_is_word;
      auto is_word = false;
      static const std::string empty;
      // The captured text does not outlive its capture.
      return parse_literal(s, n, vs, c, dt, lit, init_is_word, is_word, false,
                           empty, true);
    }
  }

  c.give_message(s, c.error_info.mark(), [&] {
    return "undefined back reference '$" + name_ + "'...";
  });
  return kFail;
}

inline size_t PrecedenceClimbing::parse_core(const char *s, size_t n,
                                             SemanticValues &vs, Context &c,
                                             std::any &dt) const {
  auto operand_missing = false;
  if (!rule_.is_macro || rule_.is_left_recursive) {
    return parse_expression(s, n, vs, c, dt, 0, operand_missing);
  }

  // A macro's body parses on its caller's values, unless it is left-recursive
  // (see Holder::parse_core). Fold the operands in a scope of their own, so
  // that the caller's values stay out of the actions and the fold.
  return parse_in_scope(vs, c, [&](SemanticValues &chvs) {
    return parse_expression(s, n, chvs, c, dt, 0, operand_missing);
  });
}

// Parses into a scope of its own and, on a match, hands what it produced
// over to `vs`: all its values with their tags, and its tokens.
template <typename F>
inline size_t PrecedenceClimbing::parse_in_scope(SemanticValues &vs, Context &c,
                                                 F parse) {
  auto &chvs = c.push_semantic_values_scope();
  auto se = scope_exit([&]() { c.pop_semantic_values_scope(); });
  auto len = parse(chvs);
  if (success(len)) {
    if (vs.holds_rule_values_) { append_values(vs, chvs); }
    if (vs.is_read_) {
      vs.tokens.insert(vs.tokens.end(), chvs.tokens.begin(), chvs.tokens.end());
    }
  }
  return len;
}

inline size_t PrecedenceClimbing::parse_expression(
    const char *s, size_t n, SemanticValues &vs, Context &c, std::any &dt,
    size_t min_prec, bool &operand_missing) const {
  auto len = atom_->parse(s, n, vs, c, dt);
  if (fail(len)) { return len; }

  auto i = len;
  while (i < n) {
    // An operator that goes unused leaves nothing behind, captures included,
    // as in the repetition `atom (binop atom)*` this parses.
    auto snap = c.snapshot(vs);
    auto used = false;
    auto se_rollback = scope_exit([&]() {
      if (!used) { c.rollback(vs, snap); }
    });

    // Popped once parsed, but read until the right operand takes its place.
    auto &chvs = c.push_semantic_values_scope();
    chvs.holds_rule_tokens_ = true;
    size_t op_len;
    {
      auto se = scope_exit([&]() { c.pop_semantic_values_scope(); });
      op_len = binop_->parse(s + i, n - i, chvs, c, dt);
    }

    if (fail(op_len) || chvs.tokens.empty()) { break; }

    auto it = info_.find(chvs.tokens.front());
    if (it == info_.end()) { break; }

    auto level = std::get<0>(it->second);
    auto assoc = std::get<1>(it->second);

    if (level < min_prec) { break; }

    append_values(vs, chvs);
    i += op_len;

    auto next_min_prec = level;
    if (assoc == 'L') { next_min_prec = level + 1; }

    // The right operand folds its own operators, so it parses in a scope of
    // its own. It nests the way a rule match does, so it counts as one: a
    // chain of right-associative operators is as deep as it is long.
    auto rhs_len = parse_in_scope(vs, c, [&](SemanticValues &rhs) {
      auto nesting = c.nest(s + i, rule_);
      return parse_expression(s + i, n - i, rhs, c, dt, next_min_prec,
                              operand_missing);
    });

    // Without a right operand the operator goes unused, as the repetition
    // stops before it.
    if (fail(rhs_len)) {
      i -= op_len;
      operand_missing = true;
      break;
    }
    i += rhs_len;

    // The result stands for the operands folded into it. What an action
    // returns is this rule's value and carries its tag, as Holder::parse_core
    // tags a rule's value; without an action the first value stands for the
    // fold with its own tag, as Holder::reduce hands it over. After a
    // recovered error no value is built, as for a rule.
    std::any val;
    auto tag = rule_.tag();
    if (!c.recovered) {
      if (rule_.action) {
        // As Holder::parse_core sets them for a rule whose body is no choice
        vs.sv_ = std::string_view(s, i);
        vs.name_ = &rule_.name;
        vs.choice_count_ = 0;
        vs.choice_ = 0;
        static const std::any empty_predicate_data;
        val = rule_.action(vs, dt, empty_predicate_data);
      } else if (!vs.empty()) {
        val = std::move(vs[0]);
        tag = vs.tags[0];
      }
    }
    vs.clear();
    vs.tags.clear();
    vs.emplace_back(std::move(val));
    vs.tags.emplace_back(tag);
    used = true;

    // Like a repetition (see Repetition::parse_core), a round that consumes
    // nothing ends the loop.
    if (operand_missing || (op_len == 0 && rhs_len == 0)) { break; }
  }

  return i;
}

inline size_t Recovery::parse_core(const char *s, size_t n,
                                   SemanticValues & /*vs*/, Context &c,
                                   std::any & /*dt*/) const {
  const auto *label = this->label();
  const auto reports =
      c.reports_here() && !c.reported_recoveries.count({this, s});

  // Custom error message
  if (reports && label && !label->error_message.empty()) {
    c.error_info.set_message(
        s, resolve_capture_placeholders(label->error_message, c), label->name);
  }

  // Recovery
  auto len = kFail;
  {
    auto muted = c.mute_errors();
    Context::TraceMute mute(c);

    std::any dummy_dt;
    len = c.parse_thrown_away(
        [&](SemanticValues &vs) { return ope_->parse(s, n, vs, c, dummy_dt); });
  }

  // Where errors are muted, nothing was reported to recover from.
  if (success(len) && !c.mutes_errors) {
    c.recovered = true;

    if (reports) {
      c.error_info.output_log(c.log, c.error_reporter, c.s, c.l,
                              label ? label->name : std::string());
      c.error_info.clear();
      c.reported_recoveries.emplace(this, s);
    }
  }

  // Cut
  if (!c.cut_stack.empty()) { c.cut_stack.back() = true; }

  return len;
}

#define PEGLIB_ACCEPT(T)                                                       \
  inline void T::accept(Visitor &v) { v.visit(*this); }
PEGLIB_OPE_TYPES(PEGLIB_ACCEPT)
#undef PEGLIB_ACCEPT
#undef PEGLIB_OPE_TYPES

inline void AssignIDToDefinition::visit(Holder &ope) {
  auto p = static_cast<void *>(ope.outer_);
  if (ids.count(p)) { return; }
  auto order = ids.size();
  ids[p] = order;
  ope.ope_->accept(*this);
}

inline void AssignIDToDefinition::visit(Reference &ope) {
  if (ope.rule_) {
    for (const auto &arg : ope.args_) {
      arg->accept(*this);
    }
    ope.rule_->accept(*this);
  }
}

inline void TokenChecker::visit(Reference &ope) {
  if (ope.is_macro_) {
    for (const auto &arg : ope.args_) {
      arg->accept(*this);
    }
  } else {
    has_rule_ = true;
  }
}

// A rule that a combinator embeds, without a Reference. Going into it would
// never end for a rule that reaches itself at its own start.
inline void ComputeCanBeEmpty::visit(Holder &ope) {
  result_ = ope.outer_->can_be_empty;
}

inline void ComputeCanBeEmpty::visit(Reference &ope) {
  result_ = ope.rule_ && ope.rule_->can_be_empty;
}

inline void DetectLeftRecursion::visit(Reference &ope) {
  // Macro parameter reference: what it denotes lives in an enclosing
  // instantiation (e.g. B(X) <- C(X) where X is itself a param ref).
  auto param = !ope.rule_ && !macro_args_stack_.empty()
                   ? resolve_macro_arg(ope.iarg_)
                   : ResolvedArg{};

  if (ope.rule_ == rule_) {
    error_s = ope.s_;
  } else if (param.ope) {
    visit_argument(param);
    if (done_ == false) { return; }
  } else if (ope.is_macro_ &&
             macro_args_stack_.size() >= max_macro_inst_depth) {
    // Unbounded instantiation chain; stop descending.
  } else if (ope.rule_ &&
             refs_
                 .emplace(ope.rule_, ope.is_macro_ ? intern_macro_inst(ope) : 0)
                 .second) {
    if (ope.is_macro_) { macro_args_stack_.push_back(&ope.args_); }
    ope.rule_->accept(*this);
    if (ope.is_macro_) { macro_args_stack_.pop_back(); }
    if (done_ == false) { return; }
  }
  // If the referenced rule can match empty, don't mark as done —
  // the sequence may continue past this element to find LR.
  if (!ope.rule_ && !macro_args_stack_.empty()) {
    if (param.ope) {
      // ComputeCanBeEmpty never consults the frame stack, so the scope it
      // runs in cannot matter.
      done_ = !ComputeCanBeEmpty::check(*param.ope);
    } else {
      done_ = true;
    }
  } else {
    done_ = !(ope.rule_ && ope.rule_->can_be_empty);
  }
}

// A rule that a combinator embeds, as a rule referred to without arguments.
inline void DetectLeftRecursion::visit(WeakHolder &ope) {
  auto holder = std::dynamic_pointer_cast<Holder>(ope.weak_.lock());
  if (!holder) { return; }
  const auto &rule = *holder->outer_;
  if (refs_.emplace(&rule, 0).second) {
    holder->accept(*this);
    if (done_ == false) { return; }
  }
  done_ = !rule.can_be_empty;
}

inline size_t DetectLeftRecursion::intern_macro_inst(const Reference &ope) {
  // Resolve bare parameter references to what the enclosing instantiation was
  // given, so a macro passing its own parameter through interns to the same
  // instantiation instead of a fresh one at every nesting level.
  std::vector<std::shared_ptr<Ope>> args;
  args.reserve(ope.args_.size());
  for (const auto &arg : ope.args_) {
    auto ref = dynamic_cast<Reference *>(arg.get());
    auto resolved = ref && !ref->rule_ && !macro_args_stack_.empty()
                        ? resolve_macro_arg(ref->iarg_).ope
                        : nullptr;
    args.push_back(resolved ? resolved : arg);
  }
  auto [it, inserted] = macro_inst_ids_.emplace(macro_inst_key(ope.rule_, args),
                                                next_macro_inst_);
  if (inserted) { next_macro_inst_++; }
  return it->second;
}

// An argument written in a macro body can name the parameters of that macro,
// and would be walked again for every use of them, at every level. Read again
// in the same scope, it is not walked again: the first walk's result stands.
// Walked again, it would stop at the rules the first walk went through, and
// could take what the first walk found can match empty to consume input. An
// argument written outside a macro body names no parameter, and walking it
// again costs less than keeping its result.
inline void DetectLeftRecursion::visit_argument(const ResolvedArg &arg) {
  if (arg.depth == 0) {
    visit_in_defining_scope(arg);
    return;
  }
  std::vector<const void *> key(macro_args_stack_.begin(),
                                macro_args_stack_.begin() + arg.depth);
  key.push_back(arg.ope.get());
  auto [it, first] = args_done_.emplace(std::move(key), false);
  if (first) {
    visit_in_defining_scope(arg);
    it->second = done_;
  } else {
    done_ = it->second;
  }
}

inline void
DetectLeftRecursion::visit_in_defining_scope(const ResolvedArg &arg) {
  // The frames below the one holding it are the scope it was written in.
  // `W(X) <- Y(X / 'x')` passes Y an argument whose own `X` means W's
  // parameter, not Y's -- leaving Y's frame visible would resolve that `X`
  // right back to `X / 'x'`, forever.
  auto saved = macro_args_stack_;
  auto se = scope_exit([&]() { macro_args_stack_ = std::move(saved); });
  macro_args_stack_.resize(arg.depth);
  arg.ope->accept(*this);
}

inline DetectLeftRecursion::ResolvedArg
DetectLeftRecursion::resolve_macro_arg(size_t iarg) const {
  for (int i = static_cast<int>(macro_args_stack_.size()) - 1; i >= 0; i--) {
    auto &args = *macro_args_stack_[i];
    if (iarg >= args.size()) { return {}; }
    auto ref = dynamic_cast<Reference *>(args[iarg].get());
    if (ref && !ref->rule_) {
      // Another param ref — resolve using parent level's args
      iarg = ref->iarg_;
      continue;
    }
    return {args[iarg], static_cast<size_t>(i)};
  }
  return {};
}

inline void FindEmptyElement::visit(Reference &ope) {
  if (!ope.rule_ || std::find(rules_on_way_.begin(), rules_on_way_.end(),
                              ope.rule_) != rules_on_way_.end()) {
    return;
  }
  assert(ope.rule_->can_be_empty);
  rules_on_way_.push_back(ope.rule_);
  rule_ref = {ope.s_, ope.name_};
  ope.rule_->accept(*this);
}

inline void DetectInfiniteLoop::visit(Reference &ope) {
  if (ope.rule_) {
    auto save = std::exchange(rule_ref_, {ope.s_, ope.name_});
    ope.rule_->accept(*this);
    rule_ref_ = std::move(save);
  }
  if (ope.is_macro_) { walk_until_error(ope.args_); }
}

inline void ReferenceChecker::visit(Reference &ope) {
  // A parameter stands for what it was given, which takes no arguments.
  const Definition *rule = nullptr;
  if (std::find(params_.begin(), params_.end(), ope.name_) == params_.end()) {
    if (!grammar_.count(ope.name_)) {
      error_s[ope.name_] = ope.s_;
      error_message[ope.name_] = "'" + ope.name_ + "' is not defined.";
      return;
    }
    referenced.insert(ope.name_);
    rule = &grammar_.at(ope.name_);
  }

  if (rule && rule->is_macro) {
    if (!ope.is_macro_ || ope.args_.size() != rule->params.size()) {
      error_s[ope.name_] = ope.s_;
      error_message[ope.name_] = "incorrect number of arguments.";
    }
  } else if (ope.is_macro_) {
    error_s[ope.name_] = ope.s_;
    error_message[ope.name_] = "'" + ope.name_ + "' is not macro.";
  }
  for (const auto &arg : ope.args_) {
    arg->accept(*this);
  }
}

inline void ComputeFirstSet::visit(Reference &ope) {
  if (!ope.rule_) {
    // Macro parameter reference — can't predict what it will match
    result_.any_char = true;
    return;
  }
  result_.failures.push_back({nullptr, ope.rule_});

  auto it = cache_.find(ope.rule_);
  FirstSet computed;
  const FirstSet *rule_fs;
  if (it != cache_.end()) {
    rule_fs = &it->second;
  } else {
    auto depth = walk_into(*ope.rule_);
    if (!depth) { return; }
    auto save = std::exchange(result_, FirstSet{});
    auto outer_least_depth_met =
        std::exchange(least_depth_met_, std::numeric_limits<size_t>::max());
    ope.rule_->accept(*this);
    computed = std::move(result_);
    result_ = std::move(save);
    refs_.pop_back();
    if (least_depth_met_ >= *depth) {
      it = cache_.try_emplace(ope.rule_, std::move(computed)).first;
      rule_fs = &it->second;
    } else {
      rule_fs = &computed;
    }
    least_depth_met_ = std::min(outer_least_depth_met, least_depth_met_);
  }

  result_.merge(*rule_fs);
  if (ope.rule_->no_whitespace && rule_fs->matches_empty_on_other_bytes) {
    add_whitespace();
  }
}

// A rule that a combinator embeds is walked into as a rule is, without the
// cache, and walked into again inside itself it is a cycle, so that what is
// computed below it is not cached as complete either.
inline void ComputeFirstSet::visit(WeakHolder &ope) {
  auto holder = std::dynamic_pointer_cast<Holder>(ope.weak_.lock());
  if (!holder) { return; }
  if (!walk_into(*holder->outer_)) { return; }
  holder->accept(*this);
  refs_.pop_back();
}

// A rule met again, when it can match empty, lets what follows it here start
// a match as well. Its first set is not worked out yet, and one that matches
// empty on other bytes can match empty (asserted in SetupFirstSets), so
// can_be_empty takes no less than the first set would.
inline std::optional<size_t>
ComputeFirstSet::walk_into(const Definition &rule) {
  auto it = std::find(refs_.begin(), refs_.end(), &rule);
  if (it == refs_.end()) {
    refs_.push_back(&rule);
    return refs_.size() - 1;
  }
  least_depth_met_ =
      std::min(least_depth_met_, static_cast<size_t>(it - refs_.begin()));
  if (rule.can_be_empty) {
    result_.matches_empty_on_other_bytes = true;
    if (rule.no_whitespace) { add_whitespace(); }
  }
  return std::nullopt;
}

inline void SetupFirstSets::visit(Reference &ope) {
  if (!ope.rule_) { return; }
  ope.rule_->accept(*this); // re-entry is guarded at the rule's Holder
}

// Guard rule setup by Definition so a SetupFirstSets shared across all rules
// visits each rule's body at most once for the whole grammar. Without this the
// per-rule setup re-walks every reachable rule once per referencing rule, which
// is O(N^2) for grammars with dense cross-references.
inline void SetupFirstSets::visit(Holder &ope) {
  if (!visited_rules_.insert(ope.outer_).second) { return; }

  auto &rule = *ope.outer_;
  ComputeFirstSet cfs(first_set_cache_);
  ope.ope_->accept(cfs);
  rule.first_set = std::move(cfs.result_);
  const auto &fs = rule.first_set;
  assert(!fs.matches_empty_on_other_bytes || rule.can_be_empty);
  auto known = !fs.any_char && !fs.matches_empty_on_other_bytes;
  rule.start_bytes = known ? fs.chars : std::bitset<256>().set();

  ope.ope_->accept(*this);
}

inline void Definition::analyze() const {
  AssignIDToDefinition vis;
  holder_->accept(vis);
  if (whitespaceOpe) { whitespaceOpe->accept(vis); }
  if (wordOpe) { wordOpe->accept(vis); }
  analysis_.has_cut = vis.has_cut;
  analysis_.has_user = vis.has_user;
  analysis_.alternative_starts.assign(vis.alternative_starts.begin(),
                                      vis.alternative_starts.end());
  analysis_.rules.resize(vis.ids.size());
  for (const auto &[ptr, order] : vis.ids) {
    analysis_.rules[order] = static_cast<Definition *>(ptr);
    analysis_.rules[order]->is_token(); // see Context::push_rule
  }
  number_new_rules();
}

// The rules an operator refers to where it does not mute errors: outside
// the operand of `!` and the expression of a recovery. A rule in a macro
// argument counts as referred to there, wherever the macro puts it.
struct CollectUnmutedRefs : public TraversalVisitor {
  using TraversalVisitor::visit;
  std::vector<const Definition *> rules;

  void visit(NotPredicate &) override {}
  void visit(Recovery &) override {}
  void visit(Holder &ope) override { rules.push_back(ope.outer_); }
  void visit(Reference &ope) override {
    if (ope.rule_) { rules.push_back(ope.rule_); }
    for (const auto &arg : ope.args_) {
      arg->accept(*this);
    }
  }
};

inline void
Definition::decide_parsed_only_where_muted(std::vector<RuleUse> &uses) const {
  const auto &a = analysis_;
  std::vector<bool> unmuted(a.rules_by_id.size(), false);
  std::vector<const Definition *> todo;
  auto reach_from = [&](Ope &ope) {
    CollectUnmutedRefs refs;
    ope.accept(refs);
    for (const auto *rule : refs.rules) {
      if (a.indexes(*rule) && !unmuted[rule->id]) {
        unmuted[rule->id] = true;
        todo.push_back(rule);
      }
    }
  };
  reach_from(*holder_);
  if (whitespaceOpe) { reach_from(*whitespaceOpe); }
  if (wordOpe) { reach_from(*wordOpe); }
  // A rule that this start rule does not index is taken to be parsed where
  // errors are not muted.
  for (const auto *rule : a.rules) {
    if (!a.indexes(*rule)) { reach_from(*rule->get_core_operator()); }
  }
  while (!todo.empty()) {
    const auto *rule = todo.back();
    todo.pop_back();
    reach_from(*rule->get_core_operator());
  }
  for (const auto *rule : a.rules_by_id) {
    if (rule) { uses[rule->id].parsed_only_where_muted = !unmuted[rule->id]; }
  }
}

// The rules an operator refers to, without walking into them, macros and
// the rules in their arguments included.
struct CollectRuleRefs : public TraversalVisitor {
  using TraversalVisitor::visit;

  void visit(AndPredicate &ope) override { thrown_away(*ope.ope_); }
  void visit(NotPredicate &ope) override { thrown_away(*ope.ope_); }
  void visit(Ignore &ope) override { thrown_away(*ope.ope_); }
  void visit(Recovery &ope) override { thrown_away(*ope.ope_); }
  void visit(Holder &ope) override { add(ope.outer_); }
  void visit(Reference &ope) override {
    if (ope.rule_) {
      add(ope.rule_);
      if (ope.rule_->is_macro) { calls_macro = true; }
    }
    for (const auto &arg : ope.args_) {
      read(*arg);
    }
  }
  void visit(PrecedenceClimbing &ope) override {
    has_precedence = true;
    ope.atom_->accept(*this);
    read(*ope.binop_);
  }

  // Every rule, those in a lookahead or an ignored operator too.
  std::vector<Definition *> rules;
  // Of those, the rules whose values land in the scope the operator is
  // parsed into: not those whose match is thrown away.
  std::vector<Definition *> value_rules;
  // The rules whose values are read whatever that scope is to its reader:
  // those in a macro argument, which land where the macro puts them, and a
  // precedence's operator, whose action runs as it hands over its token.
  std::vector<Definition *> read_rules;
  bool has_precedence = false; // which reads the scope it folds
  // A macro's body parses in the scope too, putting there the values of
  // rules not listed above.
  bool calls_macro = false;

private:
  void add(Definition *rule) {
    rules.push_back(rule);
    if (!thrown_away_) { value_rules.push_back(rule); }
    if (reading_) { read_rules.push_back(rule); }
  }
  void read(Ope &ope) {
    reading_++;
    ope.accept(*this);
    reading_--;
  }
  void thrown_away(Ope &ope) {
    thrown_away_++;
    ope.accept(*this);
    thrown_away_--;
  }
  size_t reading_ = 0;
  size_t thrown_away_ = 0;
};

// The rules a rule's body enters when the rule is entered on a byte its first
// set excludes. Nothing consumes that byte, so only what the body starts with
// runs: an alternative that cannot start with the byte is skipped, a
// lookahead's operand fails at its own start, and a macro's body runs up to
// its first parameter. Whitespace skipped after an empty match, a cut, a
// recovery, a back reference, a User operator or a macro parameter there
// would make the first set any byte (ComputeFirstSet), so none of them runs.
// The walk goes on past what can match empty, also past a lookahead that
// fails there, so it can collect rules that do not run, which only keeps a
// rule from being skipped.
struct CollectStartRegion : public TraversalVisitor {
  using TraversalVisitor::visit;

  std::vector<Definition *> entered;
  // A rule a combinator embeds, whose nullability is not worked out.
  bool unknown = false;

  void visit(Sequence &ope) override {
    for (const auto &op : ope.opes_) {
      op->accept(*this);
      if (!ComputeCanBeEmpty::check(*op)) { break; }
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (size_t i = 0; i < ope.opes_.size(); i++) {
      if (i < ope.first_sets().size()) {
        const auto &fs = ope.first_sets()[i];
        if (!fs.any_char && !fs.matches_empty_on_other_bytes) { continue; }
      }
      ope.opes_[i]->accept(*this);
    }
  }
  void visit(Holder &) override { unknown = true; }
  // A macro too: its body starts where it is called, and a parameter there
  // would make the first set any byte.
  void visit(Reference &ope) override {
    if (ope.rule_) { entered.push_back(ope.rule_); }
  }
};

// What goes unnoticed in a match of a rule depends on the callbacks of the
// rule and of the rules below it, which can be set between parses. So it is
// decided again when one of them has changed. A parse keeps the outcome it
// started with.
inline std::shared_ptr<const RuleUses>
Definition::rule_uses(bool builds_everything) const {
  std::lock_guard lock(rule_uses_mutex_);
  if (update_rule_use_inputs(builds_everything) || !rule_uses_) {
    RuleUses uses;
    uses.by_id = builds_everything
                     ? std::vector<RuleUse>(analysis_.rules_by_id.size())
                     : analyze_rule_uses();
    decide_must_be_tried(uses);
    decide_parsed_only_where_muted(uses.by_id);
    rule_uses_ = std::make_shared<const RuleUses>(std::move(uses));
  }
  return rule_uses_;
}

inline bool Definition::update_rule_use_inputs(bool builds_everything) const {
  auto changed = false;
  auto update = [&](auto &was, const auto &is) {
    if (!(was == is)) {
      was = is;
      changed = true;
    }
  };

  auto &inputs = rule_use_inputs_;
  update(inputs.builds_everything, builds_everything);
  update(inputs.packrat, enablePackratParsing);
  inputs.rules.resize(analysis_.rules.size());
  for (size_t i = 0; i < analysis_.rules.size(); i++) {
    const auto &rule = *analysis_.rules[i];
    auto callbacks = static_cast<uint8_t>(
        (rule.enter ? 1 : 0) | (rule.leave ? 2 : 0) | (rule.predicate ? 4 : 0) |
        (rule.action ? 8 : 0) | (rule.disable_action ? 16 : 0) |
        (rule.error_message.empty() ? 0 : 32) |
        (rule.ignoreSemanticValue ? 64 : 0));
    update(inputs.rules[i], std::pair(rule.action.ast_node_type(), callbacks));
  }
  return changed;
}

// With packrat, the rules to memoize have been selected.
inline std::vector<RuleUse> Definition::analyze_rule_uses() const {
  const auto &a = analysis_;
  auto count = a.rules_by_id.size();

  // A rule that this start rule does not index gets no entry.
  std::vector<RuleUse> uses(count);
  std::vector<CollectRuleRefs> refs(count);
  for (auto *rule : a.rules_by_id) {
    if (rule) { rule->get_core_operator()->accept(refs[rule->id]); }
  }

  // What the whitespace matches lands in the scope of the rule being parsed,
  // unless it is ignored, as wsp() makes it.
  CollectRuleRefs whitespace;
  if (whitespaceOpe) { whitespaceOpe->accept(whitespace); }
  for (auto &r : refs) {
    r.value_rules.insert(r.value_rules.end(), whitespace.value_rules.begin(),
                         whitespace.value_rules.end());
  }

  decide_value_use(refs, whitespace, uses);
  decide_skippable(uses);
  return uses;
}

// A rule that this start rule does not index is taken to be observed and to
// have a value.
inline void
Definition::decide_value_use(const std::vector<CollectRuleRefs> &refs,
                             const CollectRuleRefs &whitespace,
                             std::vector<RuleUse> &uses) const {
  const auto &a = analysis_;
  auto count = a.rules_by_id.size();
  std::vector<bool> observed(count, true), value_always_empty(count, false);

  for (auto *rule : a.rules_by_id) {
    if (!rule) { continue; }
    auto id = rule->id;
    const auto &rules = refs[id].value_rules;

    auto has_action = rule->action && !rule->disable_action;
    auto node_type = rule->action.ast_node_type();
    auto token_node = node_type && rule->is_token();

    // Not running an AST action goes unnoticed, unless it would have thrown
    // on a value that is not a node.
    auto values_are_nodes =
        token_node || (node_type && !refs[id].calls_macro &&
                       std::all_of(rules.begin(), rules.end(), [&](auto *r) {
                         return r->action.ast_node_type() == node_type;
                       }));

    observed[id] = rule->enter || rule->leave || rule->predicate ||
                   !rule->error_message.empty() || rule->is_left_recursive ||
                   refs[id].has_precedence || (has_action && !values_are_nodes);

    // A `leave` handler can put a value in place of the rule's.
    value_always_empty[id] =
        !rule->action && !rule->leave && !refs[id].has_precedence;

    if (rule->predicate || refs[id].has_precedence ||
        (has_action && !token_node) || (!has_action && rule->leave)) {
      uses[id].child_values = RuleUse::ChildValues::read;
    } else if (has_action) {
      uses[id].child_values = RuleUse::ChildValues::unread;
    } else {
      uses[id].child_values = RuleUse::ChildValues::passed_up;
    }
  }

  // A rule that may take a value from below may have one.
  for (auto again = true; again;) {
    again = false;
    for (auto *rule : a.rules_by_id) {
      if (!rule || !value_always_empty[rule->id]) { continue; }
      const auto &rules = refs[rule->id].value_rules;
      if (!std::all_of(rules.begin(), rules.end(), [&](auto *r) {
            return a.indexes(*r) && value_always_empty[r->id];
          })) {
        value_always_empty[rule->id] = false;
        again = true;
      }
    }
  }

  // Whether a match of the rule may be delivered to a scope that holds rule
  // values. It starts from the rules whose scope is not followed: the parse
  // result, whatever a whitespace operator, a macro or a rule that this
  // start rule does not index refers to, the rules in macro arguments and a
  // precedence's operator.
  std::vector<bool> value_read(count, false);
  auto mark_read = [&](const Definition *rule) {
    if (!a.indexes(*rule) || value_read[rule->id]) { return false; }
    value_read[rule->id] = true;
    return true;
  };
  auto mark_all_read = [&](const std::vector<Definition *> &rules) {
    for (const auto *r : rules) {
      mark_read(r);
    }
  };
  mark_read(this);
  mark_all_read(whitespace.value_rules);
  mark_all_read(whitespace.read_rules);
  for (auto *rule : a.rules) {
    if (!a.indexes(*rule)) {
      CollectRuleRefs vis;
      rule->get_core_operator()->accept(vis);
      mark_all_read(vis.value_rules);
      mark_all_read(vis.read_rules);
      continue;
    }
    mark_all_read(refs[rule->id].read_rules);
    if (rule->is_macro) { mark_all_read(refs[rule->id].value_rules); }
  }
  for (auto again = true; again;) {
    again = false;
    for (auto *rule : a.rules_by_id) {
      if (!rule) { continue; }
      auto id = rule->id;
      auto holds_rule_values = false;
      switch (uses[id].child_values) {
      case RuleUse::ChildValues::read:
        holds_rule_values = value_read[id] || observed[id];
        break;
      case RuleUse::ChildValues::unread: break;
      case RuleUse::ChildValues::passed_up:
        holds_rule_values = value_read[id] && !rule->ignoreSemanticValue;
        break;
      }
      if (!holds_rule_values) { continue; }
      // The value of a `~` rule is not delivered.
      for (const auto *r : refs[id].value_rules) {
        if (!r->ignoreSemanticValue && mark_read(r)) { again = true; }
      }
    }
  }

  for (auto *rule : a.rules_by_id) {
    if (!rule) { continue; }
    auto id = rule->id;
    auto &use = uses[id];

    // A memoized match is handed to every caller at its position, so whether
    // its value is read cannot depend on the one that makes it. Neither can
    // it for a `~` rule, whose value no caller gets.
    auto has_memo = rule->is_left_recursive ||
                    (enablePackratParsing && a.packrat_index[id] >= 0);
    auto read_nowhere =
        rule->ignoreSemanticValue || (has_memo && !value_read[id]);
    auto read_everywhere = has_memo && !read_nowhere;

    use.value_always_empty = value_always_empty[id];
    if (observed[id] || (read_everywhere && !value_always_empty[id])) {
      use.recognized = RuleUse::Recognized::never;
    } else if (value_always_empty[id] || read_nowhere) {
      use.recognized = RuleUse::Recognized::always;
    } else {
      use.recognized = RuleUse::Recognized::where_unread;
    }

    if (use.child_values == RuleUse::ChildValues::passed_up) {
      if (read_everywhere) {
        use.child_values = RuleUse::ChildValues::read;
      } else if (read_nowhere) {
        use.child_values = RuleUse::ChildValues::unread;
      }
    }
  }
}

// A rule that cannot start with the next byte is skipped instead of entered
// (see Reference::parse_dispatch) only where entering it would run no callback,
// so that skipping it goes unnoticed. Entered there, the rule fails without
// consuming anything: its enter and leave run, and so do those of the rules
// it enters at its start, whose actions and predicates run too if they can
// match empty (CollectStartRegion). An AST action depends on its values alone
// and does not count. A rule that this start rule does not index is taken to
// run callbacks.
inline void Definition::decide_skippable(std::vector<RuleUse> &uses) const {
  const auto &a = analysis_;
  auto count = a.rules_by_id.size();
  std::vector<bool> on_enter(count), on_match(count);
  for (auto *rule : a.rules_by_id) {
    if (!rule) { continue; }
    auto id = rule->id;
    on_enter[id] = rule->enter || rule->leave;
    on_match[id] = rule->predicate || (rule->action && !rule->disable_action &&
                                       !rule->action.ast_node_type());
  }

  std::vector<bool> start(count);
  std::vector<std::vector<Definition *>> entered(count);
  for (auto *rule : a.rules_by_id) {
    if (!rule) { continue; }
    auto id = rule->id;
    CollectStartRegion region;
    rule->get_core_operator()->accept(region);
    start[id] = on_enter[id] || region.unknown ||
                std::any_of(region.entered.begin(), region.entered.end(),
                            [&](const auto *r) {
                              return !a.indexes(*r) ||
                                     (on_match[r->id] && r->can_be_empty);
                            });
    entered[id] = std::move(region.entered);
  }
  spread(start, [&](size_t id) -> const auto & { return entered[id]; });

  for (auto *rule : a.rules_by_id) {
    if (rule) { uses[rule->id].skippable = !start[rule->id]; }
  }
}

// What a rule gives can be set between parses, as callbacks can.
inline void Definition::decide_must_be_tried(RuleUses &uses) const {
  const auto &a = analysis_;
  std::vector<bool> must(a.rules_by_id.size());
  std::vector<std::vector<const Definition *>> at_start(a.rules_by_id.size());
  for (auto *rule : a.rules_by_id) {
    if (!rule) { continue; }
    const auto &fs = rule->first_set;
    // Where it cannot start, a rule fails, or matches empty and runs its
    // predicate on the empty match. A macro that is not left-recursive gives
    // no message (see Holder::parse_core).
    auto gives_message =
        !rule->is_macro &&
        (fs.matches_empty_on_other_bytes ? !!rule->predicate
                                         : !rule->error_message.empty());
    must[rule->id] = gives_message || rule->is_left_recursive;
    for (const auto &f : fs.failures) {
      if (f.rule) { at_start[rule->id].push_back(f.rule); }
    }
  }
  spread(must, [&](size_t id) -> const auto & { return at_start[id]; });

  uses.some_must_be_tried = any_flagged(must, a.alternative_starts);
  for (auto *rule : a.rules_by_id) {
    if (rule) { uses.by_id[rule->id].must_be_tried = must[rule->id]; }
  }
}

// Ids are never renumbered: other parses' packrat tables are indexed by them.
inline void Definition::number_new_rules() const {
  const auto &rules = analysis_.rules;

  size_t next_id = 0;
  for (const auto *rule : rules) {
    if (rule->has_id_) { next_id = std::max(next_id, rule->id + 1); }
  }
  for (auto *rule : rules) {
    if (!rule->has_id_) {
      rule->id = next_id++;
      rule->has_id_ = true;
    }
  }

  auto &rules_by_id = analysis_.rules_by_id;
  rules_by_id.assign(next_id, nullptr);
  for (auto *rule : rules) {
    if (!rules_by_id[rule->id]) { rules_by_id[rule->id] = rule; }
  }
}

template <typename T>
inline void Context::reuse_or_parse(const char *a_s, const Definition &rule,
                                    bool rule_has_id, bool needs_token,
                                    RuleMatch &match, T parse) {
  auto call = call_of(*rule.holder_, a_s);
  if (!rule_has_id) {
    if (enters_again(!in_progress.insert(call).second)) {
      match.len = kFail;
      return;
    }
    auto se = scope_exit([&]() { in_progress.erase(call); });
    parse(match);
    return;
  }

  auto def_id = rule.id;
  assert(mutes_errors || !rule_use[def_id].parsed_only_where_muted);
  auto &active = active_pos[skips_no_whitespace][def_id];
  if (enters_again(active, a_s)) {
    match.len = kFail;
    return;
  }

  auto slot = needs_token ? -1 : cache_slot(def_id);
  size_t idx = 0;
  if (slot >= 0) {
    idx = cache_index(slot, a_s);
    if (cache_registered[idx]) {
      if (packrat_stats && def_id < packrat_stats->size()) {
        (*packrat_stats)[def_id].hits++;
      }
      if (cache_success[idx]) {
        match.len = cache_len[idx];
        if (!cache_values.find(idx, match.val)) { match.val.reset(); }
      } else {
        match.len = kFail;
      }
      if (collects_expected_tokens()) { error_info.replay(call, token_rule()); }
      return;
    }
  }
  if (enablePackratParsing && packrat_stats && def_id < packrat_stats->size()) {
    (*packrat_stats)[def_id].misses++;
  }

  {
    auto save = std::exchange(active, a_s);
    auto se = scope_exit([&]() { active = save; });
    if (slot >= 0) {
      parse_memoized(call, [&]() { parse(match); });
    } else {
      parse(match);
    }
  }

  if (slot < 0 || !makes_packrat_memo(rule_use[def_id])) { return; }
  if (success(match.len)) {
    write_packrat_cache(idx, match.len, match.val);
  } else {
    cache_registered[idx] = true;
    cache_success[idx] = false;
  }
}

template <typename T>
inline void Context::reuse_or_grow(const char *a_s, const Holder &rule,
                                   RuleMatch &match, T parse) {
  auto call = call_of(rule, a_s);

  if (auto i = growth_of(call); i != no_growth) {
    match = growing[i].seed;
    use_seed(i);
    return;
  }
  if (auto it = lr_memo.find(call);
      it != lr_memo.end() && (it->second.notes_failures || !notes_failures())) {
    const auto &m = it->second;
    match = m.match;
    if (m.built_from != no_growth) { use_seed(m.built_from); }
    if (collects_expected_tokens()) { error_info.replay(call, token_rule()); }
    return;
  }

  assert(growing.empty() || growing.back().call.pos <= a_s);
  // The seed starts as a failure. Held by index: growing grows in the parse.
  growing.push_back({call, RuleMatch(), no_growth, {}});
  auto pop = scope_exit([&]() { growing.pop_back(); });
  auto i = growing.size() - 1;

  parse_memoized(call, [&]() {
    parse(match);
    if (success(match.len)) {
      growing[i].seed = std::move(match);
      while (true) {
        forget_built(i);
        RuleMatch grown;
        parse(grown);
        if (!success(grown.len) || grown.len <= growing[i].seed.len) { break; }
        growing[i].seed = std::move(grown);
      }
      match = growing[i].seed;
    }
    forget_built(i);
  });

  // Confirmed, a failure too, so the rule is not seeded here again
  auto built_from = growing[i].built_from;
  lr_memo[call] = {std::move(growing[i].seed), built_from, notes_failures()};
  if (built_from != no_growth) { growing[built_from].built.push_back(call); }
}

// Compute which rules benefit from packrat memoization.
// A rule benefits if it's reachable from 2+ alternatives of the same
// PrioritizedChoice (backtracking will re-visit it at the same position),
// unless all of them only reach it through one shared rule, which answers
// from its own memo.
inline void Definition::select_packrat_rules() const {
  std::call_once(packrat_rules_init_, [&]() {
    auto def_count = analysis_.rules_by_id.size();
    if (def_count == 0) { return; }

    // Walks what can be invoked at the *same start position* as the given Ope
    // subtree (leftmost reachability). A packrat cache hit requires the same
    // rule to be queried twice at the same position, and in a PEG that only
    // happens when alternatives of a choice share a leftmost prefix — rules
    // reachable only past a consuming element can never be re-queried by a
    // sibling alternative.
    struct LeftmostWalker : public TraversalVisitor {
      using TraversalVisitor::visit;

      // Walk from the position element `from` starts at: element `from`
      // itself, plus what follows for as long as elements can match empty —
      // only up to (and including) the first one that must consume input.
      void collect(const std::vector<std::shared_ptr<Ope>> &opes, size_t from) {
        for (auto i = from; i < opes.size(); i++) {
          opes[i]->accept(*this);
          if (!ComputeCanBeEmpty::check(*opes[i])) { break; }
        }
      }

      void visit(Sequence &ope) override { collect(ope.opes_, 0); }
      // A recovery's expression runs only after an error, so the tries it
      // makes are no reason to memoize a rule. FindBacktrackRules skips it
      // too.
      void visit(Recovery &) override {}
    };

    // The rule IDs leftmost-reachable from the given subtree.
    struct CollectLeftmostRules : public LeftmostWalker {
      using LeftmostWalker::visit;
      std::vector<bool> reachable; // indexed by def_id
      std::vector<bool>
          visited_rules; // indexed by def_id; guards Holder cycles

      CollectLeftmostRules(size_t n)
          : reachable(n, false), visited_rules(n, false) {}

      void visit(Holder &ope) override {
        auto id = ope.outer_->id;
        if (id < reachable.size()) {
          reachable[id] = true;

          // Grammars built directly via the combinator API embed rules through
          // WeakHolder rather than Reference, so a recursive rule forms a
          // Holder cycle with no Reference to break it. Guard re-entry to avoid
          // infinite recursion (reachability is monotone, so revisiting a rule
          // we have already traversed adds nothing).
          if (visited_rules[id]) { return; }
          visited_rules[id] = true;
        }
        ope.ope_->accept(*this);
      }
      void visit(Reference &ope) override {
        if (ope.rule_ && ope.rule_->id < reachable.size() &&
            !reachable[ope.rule_->id]) {
          reachable[ope.rule_->id] = true;
          ope.rule_->accept(*this);
        }
      }
    };

    // For each leftmost-reachable rule, its gateways: the first shared rule
    // (one reachable from 2+ alternatives) on each path to it, or the rule
    // itself when a path reaches it before any shared rule.
    struct CollectGateways : public LeftmostWalker {
      using LeftmostWalker::visit;
      const std::vector<size_t> &share_count;    // indexed by def_id
      std::vector<std::vector<size_t>> gateways; // indexed by def_id
      const Definition *gateway = nullptr;       // of the path being walked

      CollectGateways(const std::vector<size_t> &share_count)
          : share_count(share_count), gateways(share_count.size()) {}

      void visit(Holder &ope) override {
        auto id = ope.outer_->id;
        if (id >= gateways.size()) { return; }

        // A rule and the gateway it is reached through are the state of the
        // walk, so the list doubles as the visited set.
        auto &list = gateways[id];
        auto through = gateway ? gateway->id : id;
        if (std::find(list.begin(), list.end(), through) != list.end()) {
          return;
        }
        list.push_back(through);

        // Only a memoized rule answers for what lies below it, and a macro
        // is never memoized.
        auto saved = gateway;
        if (!gateway && share_count[id] >= 2 && !ope.outer_->is_macro) {
          gateway = ope.outer_;
        }
        ope.ope_->accept(*this);
        gateway = saved;
      }
      void visit(Reference &ope) override {
        if (ope.rule_) { ope.rule_->accept(*this); }
      }
    };

    // Find rules that benefit: queried by 2+ alternatives of the same choice
    // at the same position
    std::vector<bool> benefits(def_count, false);

    struct FindBacktrackRules : public TraversalVisitor {
      using TraversalVisitor::visit;
      std::vector<bool> &benefits;
      size_t def_count;
      std::vector<bool> visited_rules; // indexed by def_id

      FindBacktrackRules(std::vector<bool> &b, size_t n)
          : benefits(b), def_count(n), visited_rules(n, false) {}

      using Elements = std::vector<std::shared_ptr<Ope>>;

      // An alternative's top-level elements, so a shared prefix can be walked
      // element by element. By value: this runs once per grammar.
      static Elements elements_of(const std::shared_ptr<Ope> &alt) {
        if (auto *seq = dynamic_cast<Sequence *>(alt.get())) {
          return seq->opes_;
        }
        return {alt};
      }

      // `group` holds alternatives that agree on their first `k` elements, so
      // every one of them reaches element k at the same input position — that
      // is exactly when a packrat cache entry can hit. k == 0 is the plain
      // "alternatives of one choice" case; deeper k is what a shared prefix
      // like `'(' _ PATTERN _ ',' _` hides.
      void mark_aligned(const std::vector<Elements> &group, size_t k) {
        if (group.size() < 2) { return; }

        std::vector<size_t> share_count(def_count, 0);
        for (const auto &seq : group) {
          CollectLeftmostRules clr(def_count);
          clr.collect(seq, k);
          for (size_t id = 0; id < def_count; id++) {
            if (clr.reachable[id]) { share_count[id]++; }
          }
        }

        // A shared rule that is only reached through one other shared rule is
        // not queried again here: by the time a later alternative gets to it,
        // that gateway has answered from its own memo.
        CollectGateways vis(share_count);
        for (const auto &seq : group) {
          vis.collect(seq, k);
        }
        for (size_t id = 0; id < def_count; id++) {
          const auto &gateways = vis.gateways[id];
          if (share_count[id] >= 2 &&
              (gateways.size() != 1 || gateways[0] == id)) {
            benefits[id] = true;
          }
        }

        // Only alternatives that also agree on element k stay aligned past it.
        std::map<std::string, std::vector<Elements>> aligned;
        for (const auto &seq : group) {
          if (k < seq.size()) {
            aligned[OpeSignature::get(*seq[k])].push_back(seq);
          }
        }
        for (const auto &[sig, sub] : aligned) {
          mark_aligned(sub, k + 1);
        }
      }

      void visit(PrioritizedChoice &ope) override {
        std::vector<Elements> group;
        group.reserve(ope.opes_.size());
        for (const auto &op : ope.opes_) {
          group.push_back(elements_of(op));
        }
        mark_aligned(group, 0);

        // Recurse into alternatives
        for (auto &op : ope.opes_) {
          op->accept(*this);
        }
      }
      void visit(Holder &ope) override {
        auto id = ope.outer_->id;
        if (id < visited_rules.size() && !visited_rules[id]) {
          visited_rules[id] = true;
          ope.ope_->accept(*this);
        }
      }
      void visit(Reference &ope) override {
        if (ope.rule_) { ope.rule_->accept(*this); }
      }
      void visit(Recovery &) override {} // as LeftmostWalker does
    };

    // Neither the whitespace nor the word check makes a packrat memo
    // (Context::makes_packrat_memo, Context::detached), so they are not
    // walked.
    FindBacktrackRules finder(benefits, def_count);
    holder_->accept(finder);

    // The cache keeps a match's length and value, not what else its parse
    // did to the parse around it: the captures it recorded, which a back
    // reference reads, or a cut, or a recovery, that commits the choice
    // around the rule. A rule whose match may do either, itself or through
    // the rules below it, is therefore not memoized. Nor is any rule when the
    // whitespace skipping may do either, since a literal skips whitespace
    // after it in whatever rule it is in.
    struct Effects {
      bool captures = false;
      bool cuts = false; // the choice around the match
    };
    struct CollectEffects : public TraversalVisitor {
      using TraversalVisitor::visit;
      const StartRuleAnalysis &analysis;
      const std::vector<Effects> &rules; // by id: what is known of them
      Effects result;

      CollectEffects(const StartRuleAnalysis &analysis,
                     const std::vector<Effects> &rules)
          : analysis(analysis), rules(rules) {}

      void visit(Capture &) override { result.captures = true; }
      void visit(BackReference &) override { result.captures = true; }
      // A choice keeps the cuts made in it, unless it is a label's, and so
      // does a predicate (Context::contain_cuts).
      void visit(PrioritizedChoice &ope) override {
        if (!ope.for_label_) { cut_keepers_++; }
        TraversalVisitor::visit(ope);
        if (!ope.for_label_) { cut_keepers_--; }
      }
      void visit(AndPredicate &ope) override { keep_cuts_in(*ope.ope_); }
      void visit(NotPredicate &ope) override { keep_cuts_in(*ope.ope_); }
      void visit(Cut &) override { cut(); }
      void visit(Recovery &ope) override {
        cut();
        TraversalVisitor::visit(ope);
      }
      void visit(Holder &ope) override { add(*ope.outer_); }
      void visit(Reference &ope) override {
        if (ope.rule_) { add(*ope.rule_); }
        for (const auto &arg : ope.args_) {
          arg->accept(*this);
        }
      }

    private:
      size_t cut_keepers_ = 0;
      void keep_cuts_in(Ope &ope) {
        cut_keepers_++;
        ope.accept(*this);
        cut_keepers_--;
      }
      void cut() {
        if (!cut_keepers_) { result.cuts = true; }
      }
      // A rule this analysis does not index is taken to do both.
      void add(const Definition &rule) {
        auto known = analysis.indexes(rule);
        if (!known || rules[rule.id].captures) { result.captures = true; }
        if (!known || rules[rule.id].cuts) { cut(); }
      }
    };

    std::vector<Effects> effects(def_count);
    for (auto changed = true; changed;) {
      changed = false;
      for (const auto *rule : analysis_.rules) {
        if (!analysis_.indexes(*rule)) { continue; }
        CollectEffects vis(analysis_, effects);
        rule->get_core_operator()->accept(vis);
        auto &e = effects[rule->id];
        if (vis.result.captures > e.captures || vis.result.cuts > e.cuts) {
          e = vis.result;
          changed = true;
        }
      }
    }
    CollectEffects whitespace(analysis_, effects);
    if (whitespaceOpe) { whitespaceOpe->accept(whitespace); }
    const auto &ws = whitespace.result;
    for (size_t id = 0; id < def_count; id++) {
      const auto &e = effects[id];
      if (ws.captures || ws.cuts || e.captures || e.cuts) {
        benefits[id] = false;
      }
    }

    // Compact index: def_id -> slot in the cache tables (-1 = not memoized)
    analysis_.packrat_index.assign(def_count, -1);
    int32_t k = 0;
    for (size_t id = 0; id < def_count; id++) {
      if (!benefits[id]) { continue; }
      // A macro parses in its caller's scope and is never memoized, and a
      // left-recursive rule keeps its matches in lr_memo.
      const auto &rule = *analysis_.rules_by_id[id];
      if (!rule.is_macro && !rule.is_left_recursive) {
        analysis_.packrat_index[id] = k++;
      }
    }
    analysis_.packrat_cached_count = static_cast<size_t>(k);
  });
}

inline void LinkReferences::visit(Reference &ope) {
  // Check if the reference is a macro parameter
  auto found_param = false;
  for (size_t i = 0; i < params_.size(); i++) {
    const auto &param = params_[i];
    if (param == ope.name_) {
      ope.iarg_ = i;
      found_param = true;
      break;
    }
  }

  // Check if the reference is a definition rule
  if (!found_param && grammar_.count(ope.name_)) {
    auto &rule = grammar_.at(ope.name_);
    ope.rule_ = &rule;
  }

  for (const auto &arg : ope.args_) {
    arg->accept(*this);
  }
}

inline void ReplaceParameters::visit(Reference &ope) {
  if (!ope.rule_) {
    resolved_ = args_[ope.iarg_];
  } else if (auto args = resolve_all(ope.args_)) {
    auto call = std::make_shared<Reference>(ope.grammar_, ope.name_, ope.s_,
                                            ope.is_macro_, std::move(*args));
    call->rule_ = ope.rule_;
    resolved_ = std::move(call);
  }
}

inline void ReplaceParameters::visit(PrecedenceClimbing &ope) {
  auto atom = resolve(ope.atom_);
  auto binop = resolve(ope.binop_);
  if (atom != ope.atom_ || binop != ope.binop_) {
    resolved_ = pre(atom, binop, ope.info_, ope.rule_);
  }
}

// A parse enters the start rule, %whitespace and %word with nothing to fill
// a macro's parameters with, and the start rule cannot be ignored. Returns
// the rule that breaks this and why, if one does.
inline std::pair<const Definition *, std::string>
entry_rule_error(const Grammar &g, const Definition &start) {
  if (start.ignoreSemanticValue) {
    return {&start,
            "ignore operator cannot be applied to '" + start.name + "'."};
  }
  if (start.is_macro) {
    return {&start, "the macro '" + start.name + "' cannot be the start rule."};
  }
  for (auto name : {WHITESPACE_DEFINITION_NAME, WORD_DEFINITION_NAME}) {
    if (auto it = g.find(name); it != g.end() && it->second.is_macro) {
      return {&it->second, "'" + it->first + "' cannot be a macro."};
    }
  }
  return {};
}

// Works out which rules can match empty, the rules made with the combinators
// that the rules embed included. The fixed point starts from none, so that it
// is the least one, whatever the rules held before.
inline void work_out_can_be_empty(Grammar &g) {
  CollectWalkedRules walked;
  for (auto &[name, rule] : g) {
    rule.accept(walked);
  }
  for (auto *rule : walked.rules) {
    rule->can_be_empty = false;
  }
  for (auto changed = true; changed;) {
    changed = false;
    for (auto *rule : walked.rules) {
      auto can_be_empty = ComputeCanBeEmpty::check(*rule->get_core_operator());
      if (can_be_empty != rule->can_be_empty) {
        rule->can_be_empty = can_be_empty;
        changed = true;
      }
    }
  }
}

// What a loaded grammar is finished with: the start rule skips the
// %whitespace and checks the %word the grammar defines, and every rule gets
// its first sets and fast loops (SetupFirstSets). One visitor walks all
// rules, so that each rule's first sets are worked out once. Which rules can
// match empty must be worked out before (work_out_can_be_empty).
inline void finish_grammar(Grammar &g, Definition &start_rule) {
  if (auto it = g.find(WHITESPACE_DEFINITION_NAME); it != g.end()) {
    start_rule.whitespaceOpe = wsp(it->second.get_core_operator());
  }
  if (auto it = g.find(WORD_DEFINITION_NAME); it != g.end()) {
    start_rule.wordOpe = it->second.get_core_operator();
  }
  SetupFirstSets vis;
  for (auto &x : g) {
    x.second.accept(vis);
  }
}

/*-----------------------------------------------------------------------------
 *  Grammar serialization
 *
 *  Serialize a compiled Grammar (the operator tree) to a byte blob and back,
 *  letting an application skip the meta-parse on startup by embedding a
 *  prebuilt blob. Structure only: semantic callbacks (actions / enter / leave /
 *  predicate, attached by enable_ast() etc.) are NOT serialized and must be
 *  re-applied after deserialize. References resolve by name (no pointer fixup);
 *  first-sets (O(N)) and which rules can match empty are recomputed on load.
 *  The `precedence` instruction is supported (its operator table is
 *  structural).
 *  Grammars using the `User` operator or a Capture with a match action are
 *  rejected. The blob is specific to this peglib version's layout.
 *---------------------------------------------------------------------------*/

struct GrammarBlob {
  // The values are what a blob stores, so they only ever grow at the end.
  enum Tag : uint8_t {
    T_Sequence = 0,
    T_Choice = 1,
    T_Repetition = 2,
    T_And = 3,
    T_Not = 4,
    T_Dictionary = 5,
    T_Literal = 6,
    T_CharClass = 7,
    T_Char = 8,
    T_AnyChar = 9,
    T_CaptureScope = 10,
    T_Capture = 11,
    T_TokenBoundary = 12,
    T_Ignore = 13,
    T_BackRef = 14,
    T_Reference = 15,
    T_Whitespace = 16,
    T_Recovery = 17,
    T_Cut = 18,
    T_PrecedenceClimbing = 19,
    // 20 marked a missing operator, which no blob may hold any more.
  };

  // The bits of the two flag bytes a rule is stored with, which only ever
  // grow: the first byte is full, so a new one goes in the second.
  enum Flag : uint8_t {
    F_IgnoreSemanticValue = 1,
    F_IsMacro = 2,
    F_NoAstOpt = 4,
    F_EoiCheck = 8,
    F_EnablePackratParsing = 16,
    F_IsLeftRecursive = 32,
    // Earlier versions read it; this one works it out again.
    F_CanBeEmpty = 64,
    F_DisableAction = 128,
  };
  enum Flag2 : uint8_t {
    F2_NoWhitespace = 1,
  };

  struct Writer {
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u32(uint32_t v) {
      for (int i = 0; i < 4; i++)
        b.push_back((v >> (8 * i)) & 0xff);
    }
    void u64(uint64_t v) {
      for (int i = 0; i < 8; i++)
        b.push_back((v >> (8 * i)) & 0xff);
    }
    void str(const std::string &s) {
      u32((uint32_t)s.size());
      b.insert(b.end(), s.begin(), s.end());
    }
  };

  static void write_ope(Writer &w, const std::shared_ptr<Ope> &o) {
    assert(o);
    OpeWriter vis(w);
    o->accept(vis);
  }

  struct OpeWriter : StrictVisitor {
    using StrictVisitor::visit;

    explicit OpeWriter(Writer &w) : w(w) {}

    void visit(Sequence &x) override {
      w.u8(T_Sequence);
      list(x.opes_);
    }
    void visit(PrioritizedChoice &x) override {
      w.u8(T_Choice);
      w.u8(x.for_label_ ? 1 : 0);
      list(x.opes_);
    }
    void visit(Repetition &x) override {
      w.u8(T_Repetition);
      w.u64(x.min_);
      w.u64(x.max_);
      write_ope(w, x.ope_);
    }
    void visit(AndPredicate &x) override { unary(T_And, x.ope_); }
    void visit(NotPredicate &x) override { unary(T_Not, x.ope_); }
    void visit(Dictionary &x) override {
      w.u8(T_Dictionary);
      w.u8(x.trie_.ignore_case_ ? 1 : 0);
      // Recover words in their original choice-index order. The Trie stores
      // each full word's id (its index in the constructor vector), which
      // parse_core reports as vs.choice(). Iterating dic_ directly yields
      // sorted key order and would renumber the choices, so place each word at
      // its id. A word listed twice leaves an empty word, which matches
      // nothing, at its later ids.
      std::vector<std::string> words(x.trie_.items_count());
      for (auto &kv : x.trie_.dic_)
        if (kv.second.match && kv.second.id < words.size())
          words[kv.second.id] = kv.first;
      w.u32((uint32_t)words.size());
      for (auto &s : words)
        w.str(s);
    }
    void visit(LiteralString &x) override {
      w.u8(T_Literal);
      w.u8(x.ignore_case_ ? 1 : 0);
      w.str(x.lit_);
    }
    void visit(CharacterClass &x) override {
      w.u8(T_CharClass);
      w.u8(x.negated_ ? 1 : 0);
      w.u8(x.ignore_case_ ? 1 : 0);
      w.u32((uint32_t)x.ranges_.size());
      for (auto &r : x.ranges_) {
        w.u32((uint32_t)r.first);
        w.u32((uint32_t)r.second);
      }
    }
    void visit(Character &x) override {
      w.u8(T_Char);
      w.u32((uint32_t)x.ch_);
    }
    void visit(AnyCharacter &) override { w.u8(T_AnyChar); }
    void visit(CaptureScope &x) override { unary(T_CaptureScope, x.ope_); }
    void visit(Capture &x) override {
      if (x.match_action_) {
        throw std::runtime_error(
            "GrammarBlob: Capture with a match action is not serializable");
      }
      unary(T_Capture, x.ope_);
    }
    void visit(TokenBoundary &x) override { unary(T_TokenBoundary, x.ope_); }
    void visit(Ignore &x) override { unary(T_Ignore, x.ope_); }
    void visit(BackReference &x) override {
      w.u8(T_BackRef);
      w.str(x.name_);
    }
    void visit(Reference &x) override {
      w.u8(T_Reference);
      w.u8(x.is_macro_ ? 1 : 0);
      w.str(x.name_);
      list(x.args_);
    }
    void visit(Whitespace &x) override { unary(T_Whitespace, x.ope_); }
    void visit(Recovery &x) override { unary(T_Recovery, x.ope_); }
    void visit(Cut &) override { w.u8(T_Cut); }
    void visit(PrecedenceClimbing &x) override {
      w.u8(T_PrecedenceClimbing);
      write_ope(w, x.atom_);
      write_ope(w, x.binop_);
      w.u32((uint32_t)x.info_.size());
      for (auto &[key, pri] : x.info_) {
        w.str(key);
        w.u64((uint64_t)pri.first);
        w.u8((uint8_t)pri.second);
      }
    }

    // A rule is written by name, so it never gets here; a User operator holds
    // code.
    void visit(Holder &) override { not_serializable(); }
    void visit(WeakHolder &) override { not_serializable(); }
    void visit(User &) override { not_serializable(); }

  private:
    void unary(Tag tag, const std::shared_ptr<Ope> &ope) {
      w.u8(tag);
      write_ope(w, ope);
    }
    void list(const std::vector<std::shared_ptr<Ope>> &opes) {
      w.u32((uint32_t)opes.size());
      for (auto &o : opes)
        write_ope(w, o);
    }
    [[noreturn]] static void not_serializable() {
      throw std::runtime_error(
          "GrammarBlob: operator not serializable (a custom User operator or "
          "a Capture with a match action)");
    }

    Writer &w;
  };

  struct Reader {
    const uint8_t *p, *end;
    size_t depth = 0; // how deeply read_ope has nested
    uint8_t u8() {
      if (p >= end)
        throw std::runtime_error("GrammarBlob: unexpected end of blob");
      return *p++;
    }
    uint32_t u32() {
      uint32_t v = 0;
      for (int i = 0; i < 4; i++)
        v |= (uint32_t)u8() << (8 * i);
      return v;
    }
    uint64_t u64() {
      uint64_t v = 0;
      for (int i = 0; i < 8; i++)
        v |= (uint64_t)u8() << (8 * i);
      return v;
    }
    std::string str() {
      uint32_t n = u32();
      if (n > static_cast<size_t>(end - p))
        throw std::runtime_error("GrammarBlob: unexpected end of blob");
      std::string s((const char *)p, (const char *)p + n);
      p += n;
      return s;
    }
  };

  // Deeper than a grammar loaded from text can nest its operators (about a
  // thousand levels), and shallow enough that a corrupt blob cannot exhaust
  // the stack.
  static constexpr size_t MAX_OPE_DEPTH = 2048;

  static std::shared_ptr<Ope> read_ope(Reader &r, Grammar &g,
                                       Definition *owner) {
    if (r.depth == MAX_OPE_DEPTH)
      throw std::runtime_error("GrammarBlob: operators nest too deeply");
    r.depth++;
    auto se = scope_exit([&]() { r.depth--; });

    switch (r.u8()) {
    case T_Sequence: {
      uint32_t n = r.u32();
      std::vector<std::shared_ptr<Ope>> v;
      for (uint32_t i = 0; i < n; i++)
        v.push_back(read_ope(r, g, owner));
      return std::make_shared<Sequence>(std::move(v));
    }
    case T_Choice: {
      bool fl = r.u8();
      uint32_t n = r.u32();
      std::vector<std::shared_ptr<Ope>> v;
      for (uint32_t i = 0; i < n; i++)
        v.push_back(read_ope(r, g, owner));
      return std::make_shared<PrioritizedChoice>(std::move(v), fl);
    }
    case T_Repetition: {
      uint64_t mn = r.u64(), mx = r.u64();
      auto o = read_ope(r, g, owner);
      return std::make_shared<Repetition>(o, mn, mx);
    }
    case T_And: return std::make_shared<AndPredicate>(read_ope(r, g, owner));
    case T_Not: return std::make_shared<NotPredicate>(read_ope(r, g, owner));
    case T_Dictionary: {
      bool ic = r.u8();
      uint32_t n = r.u32();
      std::vector<std::string> words;
      for (uint32_t i = 0; i < n; i++)
        words.push_back(r.str());
      return std::make_shared<Dictionary>(words, ic);
    }
    case T_Literal: {
      bool ic = r.u8();
      std::string s = r.str();
      return std::make_shared<LiteralString>(std::move(s), ic);
    }
    case T_CharClass: {
      bool neg = r.u8(), ic = r.u8();
      uint32_t n = r.u32();
      if (n == 0)
        throw std::runtime_error("GrammarBlob: a character class has no range");
      std::vector<std::pair<char32_t, char32_t>> ranges;
      for (uint32_t i = 0; i < n; i++) {
        auto lo = r.u32(), hi = r.u32();
        ranges.emplace_back((char32_t)lo, (char32_t)hi);
      }
      return std::make_shared<CharacterClass>(ranges, neg, ic);
    }
    case T_Char: return std::make_shared<Character>((char32_t)r.u32());
    case T_AnyChar: return std::make_shared<AnyCharacter>();
    case T_CaptureScope:
      return std::make_shared<CaptureScope>(read_ope(r, g, owner));
    case T_Capture: {
      auto o = read_ope(r, g, owner);
      return std::make_shared<Capture>(o, nullptr);
    }
    case T_TokenBoundary:
      return std::make_shared<TokenBoundary>(read_ope(r, g, owner));
    case T_Ignore: return std::make_shared<Ignore>(read_ope(r, g, owner));
    case T_BackRef: return std::make_shared<BackReference>(r.str());
    case T_Reference: {
      bool im = r.u8();
      std::string nm = r.str();
      uint32_t n = r.u32();
      std::vector<std::shared_ptr<Ope>> args;
      for (uint32_t i = 0; i < n; i++)
        args.push_back(read_ope(r, g, owner));
      return std::make_shared<Reference>(g, nm, nullptr, im, args);
    }
    case T_Whitespace:
      return std::make_shared<Whitespace>(read_ope(r, g, owner));
    case T_Recovery: return std::make_shared<Recovery>(read_ope(r, g, owner));
    case T_Cut: return std::make_shared<Cut>();
    case T_PrecedenceClimbing: {
      if (!owner) {
        throw std::runtime_error(
            "GrammarBlob: 'precedence' operator outside a rule body");
      }
      auto atom = read_ope(r, g, owner);
      auto binop = read_ope(r, g, owner);
      uint32_t n = r.u32();
      auto pc = std::make_shared<PrecedenceClimbing>(
          atom, binop, PrecedenceClimbing::BinOpeInfo{}, *owner);
      for (uint32_t i = 0; i < n; i++) {
        std::string key = r.str();
        auto level = (size_t)r.u64();
        auto assoc = (char)r.u8();
        pc->info_[std::move(key)] = std::pair(level, assoc);
      }
      return pc;
    }
    default: throw std::runtime_error("GrammarBlob: bad operator tag");
    }
  }

  static const uint32_t MAGIC = 0x50454734; // "PEG4"

  static std::vector<uint8_t> serialize(const Grammar &g,
                                        const std::string &start) {
    Writer w;
    w.u32(MAGIC);
    w.str(start);
    // Grammar is an unordered_map, whose iteration order is implementation
    // defined: walking it directly yields different bytes for the same grammar
    // on different standard libraries, so a blob generated on one platform
    // cannot be byte-compared on another. Emit the definitions by name.
    // deserialize() rebuilds the map from the names, so the order carries no
    // meaning of its own.
    // A rule without a body is one that operator[] made for a name the
    // grammar does not define.
    std::vector<const Grammar::value_type *> defs;
    defs.reserve(g.size());
    for (auto &kv : g)
      if (kv.second.get_core_operator()) defs.push_back(&kv);
    std::sort(defs.begin(), defs.end(),
              [](const auto *a, const auto *b) { return a->first < b->first; });
    w.u32((uint32_t)defs.size());
    for (auto *kv : defs) {
      const auto &name = kv->first;
      const auto &def = kv->second;
      w.str(name);
      uint8_t flags = (def.ignoreSemanticValue ? F_IgnoreSemanticValue : 0) |
                      (def.is_macro ? F_IsMacro : 0) |
                      (def.no_ast_opt ? F_NoAstOpt : 0) |
                      (def.eoi_check ? F_EoiCheck : 0) |
                      (def.enablePackratParsing ? F_EnablePackratParsing : 0) |
                      (def.is_left_recursive ? F_IsLeftRecursive : 0) |
                      (def.can_be_empty ? F_CanBeEmpty : 0) |
                      (def.disable_action ? F_DisableAction : 0);
      w.u8(flags);
      w.u8(def.no_whitespace ? F2_NoWhitespace : 0);
      w.u32((uint32_t)def.params.size());
      for (auto &s : def.params)
        w.str(s);
      w.str(def.ast_name);
      w.str(def.error_message);
      write_ope(w, def.get_core_operator());
    }
    return std::move(w.b);
  }

  static std::shared_ptr<Grammar> deserialize(const std::vector<uint8_t> &blob,
                                              std::string &start_out) {
    Reader r{blob.data(), blob.data() + blob.size()};
    if (r.u32() != MAGIC)
      throw std::runtime_error("GrammarBlob: bad magic / not a grammar blob");
    // start_out is written only once the blob has been read in full, so a bad
    // blob leaves it as it was.
    auto start = r.str();
    uint32_t ndef = r.u32();
    auto g = std::make_shared<Grammar>();
    // Create each Definition before reading its body: a PrecedenceClimbing node
    // needs a stable reference to its owning rule at construction. Grammar is a
    // node-based map, so references stay valid as later rules are inserted.
    for (uint32_t i = 0; i < ndef; i++) {
      std::string name = r.str();
      uint8_t flags = r.u8();
      uint8_t flags2 = r.u8();
      uint32_t np = r.u32();
      std::vector<std::string> params;
      for (uint32_t k = 0; k < np; k++)
        params.push_back(r.str());
      std::string ast_name = r.str();
      std::string err = r.str();

      if (np && !(flags & F_IsMacro))
        throw std::runtime_error("GrammarBlob: rule '" + name +
                                 "' has parameters but is no macro");
      auto &def = (*g)[name];
      def.name = name;
      def.ignoreSemanticValue = flags & F_IgnoreSemanticValue;
      def.is_macro = flags & F_IsMacro;
      def.no_ast_opt = flags & F_NoAstOpt;
      def.eoi_check = flags & F_EoiCheck;
      def.enablePackratParsing = flags & F_EnablePackratParsing;
      def.is_left_recursive = flags & F_IsLeftRecursive;
      def.disable_action = flags & F_DisableAction;
      def.no_whitespace = flags2 & F2_NoWhitespace;
      def.params = std::move(params);
      def.ast_name = std::move(ast_name);
      def.error_message = std::move(err);

      auto body = read_ope(r, *g, &def);
      def <= body;
    }
    if (r.p != r.end)
      throw std::runtime_error("GrammarBlob: trailing bytes after the grammar");
    if (!g->count(start))
      throw std::runtime_error("GrammarBlob: start rule '" + start +
                               "' is not defined");
    if (auto [rule, msg] = entry_rule_error(*g, (*g)[start]); rule)
      throw std::runtime_error("GrammarBlob: " + msg);
    for (auto &x : *g) {
      // A reference to a rule the blob does not define, or with the wrong
      // arguments, would be followed when the grammar is analyzed.
      ReferenceChecker check(*g, x.second.params);
      x.second.accept(check);
      if (!check.error_message.empty()) {
        throw std::runtime_error("GrammarBlob: rule '" + x.first +
                                 "': " + check.error_message.begin()->second);
      }

      LinkReferences vis(*g, x.second.params);
      x.second.accept(vis);
    }
    work_out_can_be_empty(*g);
    // The skipping is not stored: it is made again from the definitions.
    finish_grammar(*g, (*g)[start]);
    start_out = std::move(start);
    return g;
  }
};

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

using Rules = std::unordered_map<std::string, std::shared_ptr<Ope>>;

class ParserGenerator {
public:
  struct ParserContext {
    std::shared_ptr<Grammar> grammar;
    std::string start;
    bool enablePackratParsing = false;
  };

  static ParserContext parse(const char *s, size_t n, const Rules &rules,
                             Log log, std::string_view start,
                             bool enable_left_recursion = true) {
    return get_instance().perform_core(s, n, rules, log, std::string(start),
                                       enable_left_recursion);
  }

  // For debugging purpose
  static bool parse_test(const char *d, const char *s) {
    Data data;
    std::any dt = &data;

    auto n = strlen(s);
    auto r = get_instance().g[d].parse(s, n, dt);
    return r.ret && r.len == n;
  }

#if defined(__cpp_lib_char8_t)
  static bool parse_test(const char *d, const char8_t *s) {
    return parse_test(d, u8(s));
  }
#endif

private:
  static ParserGenerator &get_instance() {
    static ParserGenerator instance;
    return instance;
  }

  ParserGenerator() {
    make_grammar();
    setup_actions();
    // Apply First-Set filtering to the bootstrap meta-grammar itself so that
    // parsing a grammar (the bulk of load_grammar) skips alternatives whose
    // next byte cannot match. This is safe -- First-Set filtering only skips
    // alternatives that would have failed anyway, so no semantic action that
    // would have committed is skipped (unlike packrat, which is unsound here).
    work_out_can_be_empty(g);
    finish_grammar(g, g["Grammar"]);
  }

  struct Instruction {
    std::string type;
    std::any data;
    std::string_view sv;
  };

  struct Data {
    std::shared_ptr<Grammar> grammar;
    std::string start;
    const char *start_pos = nullptr;

    std::vector<std::pair<std::string, const char *>> duplicates_of_definition;

    std::vector<std::pair<std::string, const char *>> duplicates_of_instruction;
    std::map<std::string, std::vector<Instruction>> instructions;

    std::vector<std::pair<std::string, const char *>> undefined_back_references;
    std::vector<const char *> recovers_without_one_argument;
    std::vector<std::set<std::string_view>> captures_stack{{}};

    std::set<std::string_view> captures_in_current_definition;
    bool enablePackratParsing = true;

    Data() : grammar(std::make_shared<Grammar>()) {}
  };

  class SyntaxErrorException : public std::runtime_error {
  public:
    SyntaxErrorException(const char *what_arg, std::pair<size_t, size_t> r)
        : std::runtime_error(what_arg), r_(r) {}

    std::pair<size_t, size_t> line_info() const { return r_; }

  private:
    std::pair<size_t, size_t> r_;
  };

  void make_grammar() {
    // Setup PEG syntax parser
    g["Grammar"] <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
    // Left-factored: parse the rule name (IdentCont) once, then the macro
    // parameter list if any. `opt(Parameters)` pushes a value only for a
    // macro, and Spacing (~, no value) consumes the gap before LEFTARROW.
    g["Definition"] <= seq(g["Ignore"], g["IdentCont"], opt(g["Parameters"]),
                           g["Spacing"], g["LEFTARROW"], g["Expression"],
                           opt(g["Instruction"]));
    g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
    g["Sequence"] <= zom(cho(g["CUT"], g["Prefix"]));
    g["Prefix"] <= seq(opt(cho(g["AND"], g["NOT"])), g["SuffixWithLabel"]);
    g["SuffixWithLabel"] <=
        seq(g["Suffix"], opt(seq(g["LABEL"], g["Identifier"])));
    g["Suffix"] <= seq(g["Primary"], opt(g["Loop"]));
    g["Loop"] <= cho(g["QUESTION"], g["STAR"], g["PLUS"], g["Repetition"]);
    // Left-factored: a macro reference (`Name(args)`) and a plain reference
    // (`Name`) share the leading `Ignore IdentCont`, so parse it once and let
    // `opt(Arguments)` decide. opt() pushes the argument list only for a macro
    // reference, so vs.size() distinguishes the two in the action.
    g["Primary"] <=
        cho(seq(g["Ignore"], g["IdentCont"], opt(g["Arguments"]), g["Spacing"],
                npd(seq(opt(g["Parameters"]), g["LEFTARROW"]))),
            seq(g["OPEN"], g["Expression"], g["CLOSE"]),
            seq(g["BeginTok"], g["Expression"], g["EndTok"]), g["CapScope"],
            seq(g["BeginCap"], g["Expression"], g["EndCap"]), g["BackRef"],
            g["DictionaryI"], g["LiteralI"], g["Dictionary"], g["Literal"],
            g["NegatedClassI"], g["NegatedClass"], g["ClassI"], g["Class"],
            g["DOT"]);

    g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
    g["IdentCont"] <= tok(seq(g["IdentStart"], zom(g["IdentRest"])));

    const static std::vector<std::pair<char32_t, char32_t>> range = {
        {0x0080, 0xFFFF}};
    g["IdentStart"] <= seq(npd(lit(u8(u8"↑"))), npd(lit(u8(u8"⇑"))),
                           cho(cls("a-zA-Z_%"), cls(range)));

    g["IdentRest"] <= cho(g["IdentStart"], cls("0-9"));

    g["Dictionary"] <= seq(g["LiteralD"], oom(seq(g["PIPE"], g["LiteralD"])));

    g["DictionaryI"] <=
        seq(g["LiteralID"], oom(seq(g["PIPE"], g["LiteralID"])));

    auto lit_ope = cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))),
                           cls("'"), g["Spacing"]),
                       seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))),
                           cls("\""), g["Spacing"]));
    g["Literal"] <= lit_ope;
    g["LiteralD"] <= lit_ope;

    auto lit_case_ignore_ope =
        cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))), lit("'i"),
                g["Spacing"]),
            seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))), lit("\"i"),
                g["Spacing"]));
    g["LiteralI"] <= lit_case_ignore_ope;
    g["LiteralID"] <= lit_case_ignore_ope;

    // NOTE: The original Brian Ford's paper uses 'zom' instead of 'oom'.
    g["Class"] <= seq(chr('['), npd(chr('^')),
                      tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
                      g["Spacing"]);
    g["ClassI"] <= seq(chr('['), npd(chr('^')),
                       tok(oom(seq(npd(chr(']')), g["Range"]))), lit("]i"),
                       g["Spacing"]);

    g["NegatedClass"] <= seq(lit("[^"),
                             tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
                             g["Spacing"]);
    g["NegatedClassI"] <= seq(lit("[^"),
                              tok(oom(seq(npd(chr(']')), g["Range"]))),
                              lit("]i"), g["Spacing"]);

    // NOTE: This is different from The original Brian Ford's paper, and this
    // modification allows us to specify `[+-]` as a valid char class.
    g["Range"] <= cho(seq(g["Char"], chr('-'), npd(chr(']')), g["Char"]),
                      g["ClassEscape"], g["PosixClass"], g["Char"]);

    g["ClassEscape"] <= seq(chr('\\'), cls("dDwWsS"));
    g["PosixClass"] <=
        seq(lit("[:"), opt(chr('^')), oom(cls("a-z")), lit(":]"));

    g["Char"] <=
        cho(seq(chr('\\'), cls("fnrtv'\"[]\\^-")),
            seq(chr('\\'), cls("0-3"), cls("0-7"), cls("0-7")),
            seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
            seq(lit("\\x"), cls("0-9a-fA-F"), opt(cls("0-9a-fA-F"))),
            seq(lit("\\u"),
                cho(seq(cho(seq(chr('0'), cls("0-9a-fA-F")), lit("10")),
                        rep(cls("0-9a-fA-F"), 4, 4)),
                    rep(cls("0-9a-fA-F"), 4, 5))),
            seq(npd(chr('\\')), dot()));

    g["Repetition"] <=
        seq(g["BeginBracket"], g["RepetitionRange"], g["EndBracket"]);
    g["RepetitionRange"] <= cho(seq(g["Number"], g["COMMA"], g["Number"]),
                                seq(g["Number"], g["COMMA"]), g["Number"],
                                seq(g["COMMA"], g["Number"]));
    g["Number"] <= seq(oom(cls("0-9")), g["Spacing"]);

    g["CapScope"] <= seq(g["BeginCapScope"], g["Expression"], g["EndCapScope"]);

    g["LEFTARROW"] <= seq(cho(lit("<-"), lit(u8(u8"←"))), g["Spacing"]);
    ~g["SLASH"] <= seq(chr('/'), g["Spacing"]);
    ~g["PIPE"] <= seq(chr('|'), g["Spacing"]);
    g["AND"] <= seq(chr('&'), g["Spacing"]);
    g["NOT"] <= seq(chr('!'), g["Spacing"]);
    g["QUESTION"] <= seq(chr('?'), g["Spacing"]);
    g["STAR"] <= seq(chr('*'), g["Spacing"]);
    g["PLUS"] <= seq(chr('+'), g["Spacing"]);
    ~g["OPEN"] <= seq(chr('('), g["Spacing"]);
    ~g["CLOSE"] <= seq(chr(')'), g["Spacing"]);
    g["DOT"] <= seq(chr('.'), g["Spacing"]);

    g["CUT"] <= seq(lit(u8(u8"↑")), g["Spacing"]);
    ~g["LABEL"] <= seq(cho(chr('^'), lit(u8(u8"⇑"))), g["Spacing"]);

    ~g["Spacing"] <= zom(cho(g["Space"], g["Comment"]));
    g["Comment"] <= seq(chr('#'), zom(seq(npd(g["EndOfLine"]), dot())),
                        opt(g["EndOfLine"]));
    g["Space"] <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
    g["EndOfLine"] <= cho(lit("\r\n"), chr('\n'), chr('\r'));
    g["EndOfFile"] <= npd(dot());

    ~g["BeginTok"] <= seq(chr('<'), g["Spacing"]);
    ~g["EndTok"] <= seq(chr('>'), g["Spacing"]);

    ~g["BeginCapScope"] <= seq(chr('$'), chr('('), g["Spacing"]);
    ~g["EndCapScope"] <= seq(chr(')'), g["Spacing"]);

    g["BeginCap"] <= seq(chr('$'), tok(g["IdentCont"]), chr('<'), g["Spacing"]);
    ~g["EndCap"] <= seq(chr('>'), g["Spacing"]);

    g["BackRef"] <= seq(chr('$'), tok(g["IdentCont"]), g["Spacing"]);

    g["IGNORE"] <= chr('~');

    g["Ignore"] <= opt(g["IGNORE"]);
    g["Parameters"] <= seq(g["OPEN"], g["Identifier"],
                           zom(seq(g["COMMA"], g["Identifier"])), g["CLOSE"]);
    g["Arguments"] <= seq(g["OPEN"], g["Expression"],
                          zom(seq(g["COMMA"], g["Expression"])), g["CLOSE"]);
    ~g["COMMA"] <= seq(chr(','), g["Spacing"]);

    // Instruction grammars
    g["Instruction"] <=
        seq(g["BeginBracket"],
            opt(seq(g["InstructionItem"], zom(seq(g["InstructionItemSeparator"],
                                                  g["InstructionItem"])))),
            g["EndBracket"]);
    g["InstructionItem"] <= cho(g["PrecedenceClimbing"], g["ErrorMessage"],
                                g["NoAstOpt"], g["NoWhitespace"], g["AstName"]);
    ~g["InstructionItemSeparator"] <= seq(chr(';'), g["Spacing"]);

    ~g["SpacesZom"] <= zom(g["Space"]);
    ~g["SpacesOom"] <= oom(g["Space"]);
    ~g["BeginBracket"] <= seq(chr('{'), g["Spacing"]);
    ~g["EndBracket"] <= seq(chr('}'), g["Spacing"]);

    // PrecedenceClimbing instruction
    g["PrecedenceClimbing"] <=
        seq(lit("precedence"), g["SpacesOom"], g["PrecedenceInfo"],
            zom(seq(g["SpacesOom"], g["PrecedenceInfo"])), g["SpacesZom"]);
    g["PrecedenceInfo"] <=
        seq(g["PrecedenceAssoc"],
            oom(seq(ign(g["SpacesOom"]), g["PrecedenceOpe"])));
    g["PrecedenceOpe"] <=
        cho(seq(cls("'"),
                tok(zom(seq(npd(cho(g["Space"], cls("'"))), g["Char"]))),
                cls("'")),
            seq(cls("\""),
                tok(zom(seq(npd(cho(g["Space"], cls("\""))), g["Char"]))),
                cls("\"")),
            tok(oom(seq(npd(cho(g["PrecedenceAssoc"], g["Space"], chr('}'))),
                        dot()))));
    g["PrecedenceAssoc"] <= cls("LR");

    // Error message instruction
    g["ErrorMessage"] <= seq(lit("error_message"), g["SpacesOom"],
                             g["LiteralD"], g["SpacesZom"]);

    // No Ast node optimization instruction
    g["NoAstOpt"] <= seq(lit("no_ast_opt"), g["SpacesZom"]);

    // No whitespace skipping instruction
    g["NoWhitespace"] <= seq(lit("no_whitespace"), g["SpacesZom"]);

    // AST node name override instruction: `{ ast_name: NodeTag }`
    g["AstName"] <= seq(lit("ast_name"), g["SpacesZom"], lit(":"),
                        g["SpacesZom"], g["Identifier"], g["SpacesZom"]);

    // Set definition names
    for (auto &x : g) {
      x.second.name = x.first;
    }
  }

  void setup_actions() {
    g["Definition"] = [](const SemanticValues &vs, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);

      // Macro iff the optional Parameters matched: its value (the parameter
      // name list) then sits at vs[2]. A plain definition has LEFTARROW's value
      // there instead.
      auto is_macro = vs[2].type() == typeid(std::vector<std::string>);
      auto ignore = std::any_cast<bool>(vs[0]);
      auto name = std::any_cast<std::string>(vs[1]);

      std::vector<std::string> params;
      std::shared_ptr<Ope> ope;
      auto has_instructions = false;

      if (is_macro) {
        params = std::any_cast<std::vector<std::string>>(vs[2]);
        ope = std::any_cast<std::shared_ptr<Ope>>(vs[4]);
        if (vs.size() == 6) { has_instructions = true; }
      } else {
        ope = std::any_cast<std::shared_ptr<Ope>>(vs[3]);
        if (vs.size() == 5) { has_instructions = true; }
      }

      if (has_instructions) {
        auto index = is_macro ? 5 : 4;
        std::unordered_set<std::string> types;
        for (const auto &instruction :
             std::any_cast<std::vector<Instruction>>(vs[index])) {
          const auto &type = instruction.type;
          if (types.find(type) == types.end()) {
            data.instructions[name].push_back(instruction);
            types.insert(instruction.type);
          } else {
            data.duplicates_of_instruction.emplace_back(type,
                                                        instruction.sv.data());
          }
        }
      }

      auto &grammar = *data.grammar;
      if (!grammar.count(name)) {
        auto &rule = grammar[name];
        rule <= ope;
        rule.name = name;
        rule.s_ = vs.sv().data();
        rule.line_ = line_info(vs.ss, rule.s_);
        rule.ignoreSemanticValue = ignore;
        rule.is_macro = is_macro;
        rule.params = params;

        // Reserved `%`-prefixed rules (%whitespace, %word, ...) are directives,
        // not parseable entry points, so they must not become the start rule.
        if (data.start.empty() && name[0] != '%') {
          data.start = rule.name;
          data.start_pos = rule.s_;
        }
      } else {
        data.duplicates_of_definition.emplace_back(name, vs.sv().data());
      }
    };

    g["Definition"].enter = [](const Context & /*c*/, const char * /*s*/,
                               size_t /*n*/, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);
      data.captures_in_current_definition.clear();
    };

    g["Expression"] = [](const SemanticValues &vs) {
      if (vs.size() == 1) {
        return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      } else {
        std::vector<std::shared_ptr<Ope>> opes;
        for (auto i = 0u; i < vs.size(); i++) {
          opes.emplace_back(std::any_cast<std::shared_ptr<Ope>>(vs[i]));
        }
        const std::shared_ptr<Ope> ope =
            std::make_shared<PrioritizedChoice>(opes);
        return ope;
      }
    };

    g["Sequence"] = [](const SemanticValues &vs) {
      if (vs.empty()) {
        return npd(lit(""));
      } else if (vs.size() == 1) {
        return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      } else {
        std::vector<std::shared_ptr<Ope>> opes;
        for (const auto &x : vs) {
          opes.emplace_back(std::any_cast<std::shared_ptr<Ope>>(x));
        }
        const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
        return ope;
      }
    };

    g["Prefix"] = [](const SemanticValues &vs) {
      std::shared_ptr<Ope> ope;
      if (vs.size() == 1) {
        ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      } else {
        assert(vs.size() == 2);
        auto tok = std::any_cast<char>(vs[0]);
        ope = std::any_cast<std::shared_ptr<Ope>>(vs[1]);
        if (tok == '&') {
          ope = apd(ope);
        } else { // '!'
          ope = npd(ope);
        }
      }
      return ope;
    };

    g["SuffixWithLabel"] = [](const SemanticValues &vs, std::any &dt) {
      auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      if (vs.size() == 1) {
        return ope;
      } else {
        assert(vs.size() == 2);
        auto &data = *std::any_cast<Data *>(dt);
        const auto &ident = std::any_cast<std::string>(vs[1]);
        auto label = ref(*data.grammar, ident, vs.sv().data(), false, {});
        return cho4label_(ope, rec(label));
      }
    };

    struct Loop {
      enum class Type { opt = 0, zom, oom, rep };
      Type type;
      std::pair<size_t, size_t> range;
    };

    g["Suffix"] = [](const SemanticValues &vs) {
      auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      if (vs.size() == 1) {
        return ope;
      } else {
        assert(vs.size() == 2);
        auto loop = std::any_cast<Loop>(vs[1]);
        switch (loop.type) {
        case Loop::Type::opt: return opt(ope);
        case Loop::Type::zom: return zom(ope);
        case Loop::Type::oom: return oom(ope);
        default: // Regex-like repetition
          return rep(ope, loop.range.first, loop.range.second);
        }
      }
    };

    g["Loop"] = [](const SemanticValues &vs) {
      switch (vs.choice()) {
      case 0: // Option
        return Loop{Loop::Type::opt, std::pair<size_t, size_t>()};
      case 1: // Zero or More
        return Loop{Loop::Type::zom, std::pair<size_t, size_t>()};
      case 2: // One or More
        return Loop{Loop::Type::oom, std::pair<size_t, size_t>()};
      default: // Regex-like repetition
        return Loop{Loop::Type::rep,
                    std::any_cast<std::pair<size_t, size_t>>(vs[0])};
      }
    };

    g["Primary"] = [](const SemanticValues &vs, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);

      switch (vs.choice()) {
      case 0: { // Reference / Macro reference (left-factored)
        // Macro reference iff opt(Arguments) matched and pushed the arg list.
        auto is_macro = vs.size() > 2;
        auto ignore = std::any_cast<bool>(vs[0]);
        const auto &ident = std::any_cast<std::string>(vs[1]);

        std::vector<std::shared_ptr<Ope>> args;
        if (is_macro) {
          args = std::any_cast<std::vector<std::shared_ptr<Ope>>>(vs[2]);
        }

        std::shared_ptr<Ope> ope;
        if (ident == RECOVER_DEFINITION_NAME) {
          // Reported once the grammar has parsed, as for a macro, so that a
          // syntax error in what was meant as its argument comes first.
          if (args.size() != 1) {
            data.recovers_without_one_argument.push_back(vs.sv().data());
            ope = lit("");
          } else {
            ope = rec(args[0]);
          }
        } else {
          ope = ref(*data.grammar, ident, vs.sv().data(), is_macro, args);
        }

        if (ignore) {
          return ign(ope);
        } else {
          return ope;
        }
      }
      case 1: { // (Expression)
        return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      }
      case 2: { // TokenBoundary
        return tok(std::any_cast<std::shared_ptr<Ope>>(vs[0]));
      }
      case 3: { // CaptureScope
        return csc(std::any_cast<std::shared_ptr<Ope>>(vs[0]));
      }
      case 4: { // Capture
        const auto &name = std::any_cast<std::string_view>(vs[0]);
        auto ope = std::any_cast<std::shared_ptr<Ope>>(vs[1]);

        data.captures_stack.back().insert(name);
        data.captures_in_current_definition.insert(name);

        // The name is kept here, since the grammar text may be gone when
        // the grammar is used, and shared by the copies of this action.
        return cap(ope, [name = std::make_shared<const std::string>(name)](
                            const char *a_s, size_t a_n, Context &c) {
          c.capture_entries.emplace_back(*name, std::string(a_s, a_n));
        });
      }
      default: {
        return std::any_cast<std::shared_ptr<Ope>>(vs[0]);
      }
      }
    };

    g["IdentCont"] = [](const SemanticValues &vs) {
      return std::string(vs.sv().data(), vs.sv().length());
    };

    g["Dictionary"] = [](const SemanticValues &vs) {
      auto items = vs.transform<std::string>();
      return dic(items, false);
    };
    g["DictionaryI"] = [](const SemanticValues &vs) {
      auto items = vs.transform<std::string>();
      return dic(items, true);
    };

    g["Literal"] = [](const SemanticValues &vs) {
      const auto &tok = vs.tokens.front();
      return lit(resolve_escape_sequence(tok.data(), tok.size()));
    };
    g["LiteralI"] = [](const SemanticValues &vs) {
      const auto &tok = vs.tokens.front();
      return liti(resolve_escape_sequence(tok.data(), tok.size()));
    };
    g["LiteralD"] = [](const SemanticValues &vs) {
      auto &tok = vs.tokens.front();
      return resolve_escape_sequence(tok.data(), tok.size());
    };
    g["LiteralID"] = [](const SemanticValues &vs) {
      auto &tok = vs.tokens.front();
      return resolve_escape_sequence(tok.data(), tok.size());
    };

    // A `\d`-style escape or a POSIX class, negated once its class tells
    // whether it ignores case.
    struct PredefinedClass {
      std::vector<std::pair<char32_t, char32_t>> ranges;
      bool negated;
    };

    // A Range produces either a single range (std::pair) or a PredefinedClass.
    auto collect_ranges = [](const SemanticValues &vs, bool ignore_case) {
      std::vector<std::pair<char32_t, char32_t>> ranges;
      for (const auto &v : vs) {
        if (v.type() == typeid(std::pair<char32_t, char32_t>)) {
          ranges.push_back(std::any_cast<std::pair<char32_t, char32_t>>(v));
        } else {
          auto [named, negated] = std::any_cast<PredefinedClass>(v);
          if (negated) {
            // Case is ignored before the class is negated.
            if (ignore_case) { named = add_other_case(named); }
            named = complement_character_ranges(named);
          }
          ranges.insert(ranges.end(), named.begin(), named.end());
        }
      }
      return ranges;
    };

    g["Class"] = [collect_ranges](const SemanticValues &vs) {
      return cls(collect_ranges(vs, false));
    };
    g["ClassI"] = [collect_ranges](const SemanticValues &vs) {
      return cls(collect_ranges(vs, true), true);
    };
    g["NegatedClass"] = [collect_ranges](const SemanticValues &vs) {
      return ncls(collect_ranges(vs, false));
    };
    g["NegatedClassI"] = [collect_ranges](const SemanticValues &vs) {
      return ncls(collect_ranges(vs, true), true);
    };
    g["Range"] = [](const SemanticValues &vs) -> std::any {
      switch (vs.choice()) {
      case 0: {
        auto s1 = std::any_cast<std::string>(vs[0]);
        auto s2 = std::any_cast<std::string>(vs[1]);
        auto cp1 = decode_codepoint(s1.data(), s1.length());
        auto cp2 = decode_codepoint(s2.data(), s2.length());
        if (cp1 > cp2) {
          throw SyntaxErrorException("character range is out of order...",
                                     vs.line_info());
        }
        return std::pair(cp1, cp2);
      }
      case 1: // ClassEscape
      case 2: // PosixClass
        return vs[0];
      default: { // Char
        auto s = std::any_cast<std::string>(vs[0]);
        auto cp = decode_codepoint(s.data(), s.length());
        return std::pair(cp, cp);
      }
      }
    };
    g["ClassEscape"] = [](const SemanticValues &vs) {
      auto ch = vs.sv()[1];
      const char *name = nullptr;
      switch (ch) {
      case 'd':
      case 'D': name = "digit"; break;
      case 's':
      case 'S': name = "space"; break;
      default: name = "word"; break;
      }
      return PredefinedClass{*predefined_character_class(name),
                             ch == 'D' || ch == 'S' || ch == 'W'};
    };
    g["PosixClass"] = [](const SemanticValues &vs) {
      auto name = vs.sv(); // `[:name:]` or `[:^name:]`
      name.remove_prefix(2);
      name.remove_suffix(2);
      auto negated = name.front() == '^';
      if (negated) { name.remove_prefix(1); }
      auto ranges = predefined_character_class(name);
      if (!ranges) {
        auto msg = "invalid POSIX character class '" + std::string(name) + "'";
        throw SyntaxErrorException(msg.c_str(), vs.line_info());
      }
      return PredefinedClass{*ranges, negated};
    };
    g["Char"] = [](const SemanticValues &vs) {
      return resolve_escape_sequence(vs.sv().data(), vs.sv().length());
    };

    g["RepetitionRange"] = [](const SemanticValues &vs) {
      switch (vs.choice()) {
      case 0: { // Number COMMA Number
        auto min = std::any_cast<size_t>(vs[0]);
        auto max = std::any_cast<size_t>(vs[1]);
        return std::pair(min, max);
      }
      case 1: // Number COMMA
        return std::pair(std::any_cast<size_t>(vs[0]),
                         std::numeric_limits<size_t>::max());
      case 2: { // Number
        auto n = std::any_cast<size_t>(vs[0]);
        return std::pair(n, n);
      }
      default: // COMMA Number
        return std::pair(std::numeric_limits<size_t>::min(),
                         std::any_cast<size_t>(vs[0]));
      }
    };
    g["Number"] = [](const SemanticValues &vs) {
      return vs.token_to_number<size_t>();
    };

    g["CapScope"].enter = [](const Context & /*c*/, const char * /*s*/,
                             size_t /*n*/, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);
      data.captures_stack.emplace_back();
    };
    g["CapScope"].leave = [](const Context & /*c*/, const char * /*s*/,
                             size_t /*n*/, size_t /*matchlen*/,
                             std::any & /*value*/, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);
      data.captures_stack.pop_back();
    };

    g["AND"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
    g["NOT"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
    g["QUESTION"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
    g["STAR"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
    g["PLUS"] = [](const SemanticValues &vs) { return *vs.sv().data(); };

    g["DOT"] = [](const SemanticValues & /*vs*/) { return dot(); };

    g["CUT"] = [](const SemanticValues & /*vs*/) { return cut(); };

    g["BeginCap"] = [](const SemanticValues &vs) { return vs.token(); };

    g["BackRef"] = [](const SemanticValues &vs, std::any &dt) {
      auto &data = *std::any_cast<Data *>(dt);

      // Undefined back reference check
      {
        auto found = false;
        auto it = data.captures_stack.rbegin();
        while (it != data.captures_stack.rend()) {
          if (it->find(vs.token()) != it->end()) {
            found = true;
            break;
          }
          ++it;
        }
        if (!found) {
          auto ptr = vs.token().data() - 1; // include '$' symbol
          data.undefined_back_references.emplace_back(vs.token(), ptr);
        }
      }

      // NOTE: Disable packrat parsing if a back reference is not defined in
      // captures in the current definition rule.
      if (data.captures_in_current_definition.find(vs.token()) ==
          data.captures_in_current_definition.end()) {
        data.enablePackratParsing = false;
      }

      return bkr(vs.token_to_string());
    };

    g["Ignore"] = [](const SemanticValues &vs) { return vs.size() > 0; };

    g["Parameters"] = [](const SemanticValues &vs) {
      return vs.transform<std::string>();
    };

    g["Arguments"] = [](const SemanticValues &vs) {
      return vs.transform<std::shared_ptr<Ope>>();
    };

    g["PrecedenceClimbing"] = [](const SemanticValues &vs) {
      PrecedenceClimbing::BinOpeInfo binOpeInfo;
      size_t level = 1;
      for (const auto &v : vs) {
        auto tokens = std::any_cast<std::vector<std::string_view>>(v);
        auto assoc = tokens[0][0];
        for (size_t i = 1; i < tokens.size(); i++) {
          binOpeInfo[std::string(tokens[i])] = std::pair(level, assoc);
        }
        level++;
      }
      Instruction instruction;
      instruction.type = "precedence";
      instruction.data = binOpeInfo;
      instruction.sv = vs.sv();
      return instruction;
    };
    g["PrecedenceInfo"] = [](const SemanticValues &vs) {
      return vs.transform<std::string_view>();
    };
    g["PrecedenceOpe"] = [](const SemanticValues &vs) { return vs.token(); };
    g["PrecedenceAssoc"] = [](const SemanticValues &vs) { return vs.token(); };

    g["ErrorMessage"] = [](const SemanticValues &vs) {
      Instruction instruction;
      instruction.type = "error_message";
      instruction.data = std::any_cast<std::string>(vs[0]);
      instruction.sv = vs.sv();
      return instruction;
    };

    g["NoAstOpt"] = [](const SemanticValues &vs) {
      Instruction instruction;
      instruction.type = "no_ast_opt";
      instruction.sv = vs.sv();
      return instruction;
    };

    g["NoWhitespace"] = [](const SemanticValues &vs) {
      Instruction instruction;
      instruction.type = "no_whitespace";
      instruction.sv = vs.sv();
      return instruction;
    };

    g["AstName"] = [](const SemanticValues &vs) {
      Instruction instruction;
      instruction.type = "ast_name";
      instruction.data = std::any_cast<std::string>(vs[0]);
      instruction.sv = vs.sv();
      return instruction;
    };

    g["Instruction"] = [](const SemanticValues &vs) {
      return vs.transform<Instruction>();
    };
  }

  // Reports `msg` at `pos` in the grammar `s`, if there is a log.
  static void log_at(const Log &log, const char *s, const char *pos,
                     const std::string &msg, const std::string &label = "") {
    if (!log) { return; }
    auto line = line_info(s, pos);
    log(line.first, line.second, msg, label);
  }

  bool apply_precedence_instruction(Definition &rule,
                                    const PrecedenceClimbing::BinOpeInfo &info,
                                    const char *s, const Log &log) {
    auto cannot_apply = [&]() {
      log_at(log, s, rule.s_,
             "'precedence' instruction cannot be applied to '" + rule.name +
                 "'.");
      return false;
    };

    try {
      auto &seq = dynamic_cast<Sequence &>(*rule.get_core_operator());
      auto atom = seq.opes_[0];
      auto &rep = dynamic_cast<Repetition &>(*seq.opes_[1]);
      auto &seq1 = dynamic_cast<Sequence &>(*rep.ope_);
      auto binop = seq1.opes_[0];
      auto atom1 = seq1.opes_[1];

      auto atom_name = dynamic_cast<Reference &>(*atom).name_;
      auto binop_name = dynamic_cast<Reference &>(*binop).name_;
      auto atom1_name = dynamic_cast<Reference &>(*atom1).name_;

      if (!rep.is_zom() || atom_name != atom1_name || atom_name == binop_name) {
        return cannot_apply();
      }

      rule.holder_->ope_ = pre(atom, binop, info, rule);
      rule.disable_action = true;
    } catch (...) { return cannot_apply(); }
    return true;
  }

  ParserContext perform_core(const char *s, size_t n, const Rules &rules,
                             Log log, std::string requested_start,
                             bool enable_left_recursion = true) {
    Data data;
    auto &grammar = *data.grammar;

    try {
      std::any dt = &data;
      auto r = g["Grammar"].parse(s, n, dt, nullptr, log);

      if (!r.ret) {
        if (r.error_info.message_pos) {
          log_at(log, s, r.error_info.message_pos, r.error_info.message,
                 r.error_info.label);
        } else {
          log_at(log, s, r.error_info.error_pos, "syntax error");
        }
        return {};
      }
    } catch (const SyntaxErrorException &e) {
      if (log) {
        auto line = e.line_info();
        log(line.first, line.second, e.what(), "");
      }
      return {};
    }

    // User provided rules
    for (auto [user_name, user_rule] : rules) {
      auto name = user_name;
      auto ignore = false;
      if (!name.empty() && name[0] == '~') {
        ignore = true;
        name.erase(0, 1);
      }
      if (!name.empty()) {
        auto &rule = grammar[name];
        rule <= user_rule;
        rule.name = name;
        rule.ignoreSemanticValue = ignore;
      }
    }

    // Check duplicated definitions
    auto ret = true;

    if (!data.duplicates_of_definition.empty()) {
      for (const auto &[name, ptr] : data.duplicates_of_definition) {
        log_at(log, s, ptr,
               "the definition '" + name + "' is already defined.");
      }
      ret = false;
    }

    // Check duplicated instructions
    if (!data.duplicates_of_instruction.empty()) {
      for (const auto &[type, ptr] : data.duplicates_of_instruction) {
        log_at(log, s, ptr,
               "the instruction '" + type + "' is already defined.");
      }
      ret = false;
    }

    // Check undefined back references
    if (!data.undefined_back_references.empty()) {
      for (const auto &[name, ptr] : data.undefined_back_references) {
        log_at(log, s, ptr, "the back reference '" + name + "' is undefined.");
      }
      ret = false;
    }

    for (auto ptr : data.recovers_without_one_argument) {
      log_at(log, s, ptr, "incorrect number of arguments.");
      ret = false;
    }

    // Set root definition
    auto start = data.start;

    if (!requested_start.empty()) {
      if (grammar.count(requested_start)) {
        start = requested_start;
      } else {
        log_at(log, s, s,
               "the specified start rule '" + requested_start +
                   "' is undefined.");
        ret = false;
      }
    }

    if (!ret) { return {}; }

    auto &start_rule = grammar[start];

    if (auto [rule, msg] = entry_rule_error(grammar, start_rule); rule) {
      log_at(log, s, rule->s_, msg);
      return {};
    }

    // Check missing definitions
    auto referenced = std::unordered_set<std::string>{
        WHITESPACE_DEFINITION_NAME,
        WORD_DEFINITION_NAME,
        start_rule.name,
    };

    for (auto &[_, rule] : grammar) {
      ReferenceChecker vis(grammar, rule.params);
      rule.accept(vis);
      referenced.insert(vis.referenced.begin(), vis.referenced.end());
      for (const auto &[name, ptr] : vis.error_s) {
        log_at(log, s, ptr, vis.error_message[name]);
        ret = false;
      }
    }

    for (auto &[name, rule] : grammar) {
      if (log && !referenced.count(name)) {
        log_at(log, s, rule.s_, "'" + name + "' is not referenced.");
      }
    }

    if (!ret) { return {}; }

    // Link references
    for (auto &x : grammar) {
      auto &rule = x.second;
      LinkReferences vis(grammar, rule.params);
      rule.accept(vis);
    }

    work_out_can_be_empty(grammar);

    // Check left recursion
    for (auto &[name, rule] : grammar) {
      DetectLeftRecursion vis(rule);
      rule.accept(vis);
      if (vis.error_s && enable_left_recursion) {
        rule.is_left_recursive = true;
      } else if (vis.error_s) {
        log_at(log, s, vis.error_s, "'" + name + "' is left recursive.");
        ret = false;
      }
    }

    if (!ret) { return {}; }

    // Check infinite loop
    if (detect_infiniteLoop(data, start_rule, log, s)) { return {}; }

    // Automatic whitespace skipping
    if (grammar.count(WHITESPACE_DEFINITION_NAME)) {
      for (auto &x : grammar) {
        auto &rule = x.second;
        auto ope = rule.get_core_operator();
        if (IsLiteralToken::check(*ope)) { rule <= tok(ope); }
      }

      auto &rule = grammar[WHITESPACE_DEFINITION_NAME];
      if (detect_infiniteLoop(data, rule, log, s)) { return {}; }
    }

    // Word expression
    if (grammar.count(WORD_DEFINITION_NAME)) {
      auto &rule = grammar[WORD_DEFINITION_NAME];
      if (detect_infiniteLoop(data, rule, log, s)) { return {}; }

      if (rule.can_be_empty) {
        log_at(log, s, rule.s_,
               "'" + rule.name + "' can match the empty string.");
        return {};
      }
    }

    // Apply instructions
    for (const auto &[name, instructions] : data.instructions) {
      auto &rule = grammar[name];

      for (const auto &instruction : instructions) {
        if (instruction.type == "precedence") {
          const auto &info =
              std::any_cast<PrecedenceClimbing::BinOpeInfo>(instruction.data);

          if (!apply_precedence_instruction(rule, info, s, log)) { return {}; }
        } else if (instruction.type == "error_message") {
          rule.error_message = std::any_cast<std::string>(instruction.data);
        } else if (instruction.type == "no_ast_opt") {
          rule.no_ast_opt = true;
        } else if (instruction.type == "no_whitespace") {
          rule.no_whitespace = true;
        } else if (instruction.type == "ast_name") {
          rule.ast_name = std::any_cast<std::string>(instruction.data);
        }
      }
    }

    finish_grammar(grammar, start_rule);
    return {data.grammar, start, data.enablePackratParsing};
  }

  bool detect_infiniteLoop(const Data &data, Definition &rule, const Log &log,
                           const char *s) const {
    DetectInfiniteLoop vis(data.start_pos, rule.name);
    rule.accept(vis);
    if (vis.has_error) {
      log_at(log, s, vis.error_s,
             "infinite loop is detected in '" + vis.error_name + "'.");
      return true;
    }
    return false;
  }

  Grammar g;
};

/*-----------------------------------------------------------------------------
 *  AST
 *---------------------------------------------------------------------------*/

template <typename Annotation> struct AstBase : public Annotation {
  AstBase(const char *path, size_t line, size_t column, const char *name,
          const std::vector<std::shared_ptr<AstBase>> &nodes,
          size_t position = 0, size_t length = 0, size_t choice_count = 0,
          size_t choice = 0, bool preserve_position = false)
      : path(path ? path : ""), line(line), column(column), name(name),
        position(position), length(length), choice_count(choice_count),
        choice(choice), original_name(name),
        original_choice_count(choice_count), original_choice(choice),
        tag(str2tag(name)), original_tag(tag), is_token(false),
        preserve_position(preserve_position), nodes(nodes) {}

  AstBase(const char *path, size_t line, size_t column, const char *name,
          const std::string_view &token, size_t position = 0, size_t length = 0,
          size_t choice_count = 0, size_t choice = 0,
          bool preserve_position = false)
      : path(path ? path : ""), line(line), column(column), name(name),
        position(position), length(length), choice_count(choice_count),
        choice(choice), original_name(name),
        original_choice_count(choice_count), original_choice(choice),
        tag(str2tag(name)), original_tag(tag), is_token(true),
        preserve_position(preserve_position), token(token) {}

  AstBase(const AstBase &ast, const char *original_name, size_t position = 0,
          size_t length = 0, size_t original_choice_count = 0,
          size_t original_choice = 0)
      : path(ast.path), line(ast.line), column(ast.column), name(ast.name),
        position(position), length(length), choice_count(ast.choice_count),
        choice(ast.choice), original_name(original_name),
        original_choice_count(original_choice_count),
        original_choice(original_choice), tag(ast.tag),
        original_tag(str2tag(original_name)), is_token(ast.is_token),
        preserve_position(ast.preserve_position), token(ast.token),
        nodes(ast.nodes), parent(ast.parent) {}

  AstBase(const AstBase &) = default;
  AstBase(AstBase &&) = default;

  // A tree can be deeper than the call stack allows (a long chain of
  // left-associative operators), so the nodes that die with this one are
  // released in a loop here instead of each from its parent's destructor.
  ~AstBase() {
    if (nodes.empty()) { return; }

    // Set while a destructor on this thread runs that loop: a node that dies
    // meanwhile hands its children over to it.
    static thread_local std::vector<std::shared_ptr<AstBase>> *doomed_nodes =
        nullptr;
    if (doomed_nodes) {
      for (auto &node : nodes) {
        doomed_nodes->push_back(std::move(node));
      }
      return;
    }
    auto doomed = std::move(nodes);
    doomed_nodes = &doomed;
    while (!doomed.empty()) {
      // Moved out first, as its release may add to `doomed`.
      auto node = std::move(doomed.back());
      doomed.pop_back();
    }
    doomed_nodes = nullptr;
  }

  const std::string path;
  const size_t line = 1;
  const size_t column = 1;

  const std::string name;
  size_t position;
  size_t length;
  const size_t choice_count;
  const size_t choice;
  std::string original_name;
  size_t original_choice_count;
  size_t original_choice;
  const unsigned int tag;
  unsigned int original_tag;

  const bool is_token;
  const bool preserve_position;
  const std::string_view token;

  std::vector<std::shared_ptr<AstBase<Annotation>>> nodes;
  std::weak_ptr<AstBase<Annotation>> parent;

  std::string token_to_string() const {
    assert(is_token);
    return std::string(token);
  }

  template <typename T> T token_to_number() const {
    return token_to_number_<T>(token);
  }
};

template <typename T>
void ast_to_s_core(
    const std::shared_ptr<T> &ptr, std::string &s, int level,
    const std::function<std::string(const T &ast, int level)> &fn) {
  const auto &ast = *ptr;
  for (auto i = 0; i < level; i++) {
    s += "  ";
  }
  auto name = ast.original_name;
  if (ast.original_choice_count > 0) {
    name += "/" + std::to_string(ast.original_choice);
  }
  if (ast.name != ast.original_name) { name += "[" + ast.name + "]"; }
  if (ast.is_token) {
    s += "- " + name + " (";
    s += ast.token;
    s += ")\n";
  } else {
    s += "+ " + name + "\n";
  }
  if (fn) { s += fn(ast, level + 1); }
  for (const auto &node : ast.nodes) {
    ast_to_s_core(node, s, level + 1, fn);
  }
}

template <typename T>
std::string
ast_to_s(const std::shared_ptr<T> &ptr,
         std::function<std::string(const T &ast, int level)> fn = nullptr) {
  std::string s;
  ast_to_s_core(ptr, s, 0, fn);
  return s;
}

// A copy of `child` standing in for its single-child parent. Shared by
// AstOptimizer and the collapsing AST actions so both build the same tree.
template <typename T>
std::shared_ptr<T> collapse_ast_node(const T &child, const char *name,
                                     size_t position, size_t length,
                                     size_t choice_count, size_t choice) {
  auto pos = child.preserve_position ? child.position : position;
  auto len = child.preserve_position ? child.length : length;
  auto ast = std::make_shared<T>(child, name, pos, len, choice_count, choice);
  for (auto &node : ast->nodes) {
    node->parent = ast;
  }
  return ast;
}

struct AstOptimizer {
  AstOptimizer(bool mode, const std::vector<std::string> &rules = {})
      : mode_(mode), rules_(rules) {}

  bool is_optimized(const std::string &name) const {
    auto found = std::find(rules_.begin(), rules_.end(), name) != rules_.end();
    return mode_ ? !found : found;
  }

  template <typename T>
  std::shared_ptr<T> optimize(std::shared_ptr<T> original,
                              std::shared_ptr<T> parent = nullptr) {
    if (is_optimized(original->name) && original->nodes.size() == 1) {
      return collapse_ast_node(*optimize(original->nodes[0], parent),
                               original->name.data(), original->position,
                               original->length, original->choice_count,
                               original->choice);
    }

    auto ast = std::make_shared<T>(*original);
    ast->parent = parent;
    ast->nodes.clear();
    for (const auto &node : original->nodes) {
      auto child = optimize(node, ast);
      ast->nodes.push_back(child);
    }
    return ast;
  }

private:
  const bool mode_;
  const std::vector<std::string> rules_;
};

struct EmptyType {};
using Ast = AstBase<EmptyType>;

template <typename T = Ast>
void add_ast_action(Definition &rule, bool collapse = false) {
  rule.ast_node_name = rule.node_name();
  rule.ast_node_tag = str2tag(rule.ast_node_name);
  rule.action = [&rule, collapse](const SemanticValues &vs) {
    const char *node_name = rule.ast_node_name.c_str();
    auto position = static_cast<size_t>(std::distance(vs.ss, vs.sv().data()));
    auto length = vs.sv().length();

    if (rule.is_token()) {
      auto line = vs.line_info();
      return std::make_shared<T>(
          vs.path, line.first, line.second, node_name, vs.token(), position,
          length, vs.choice_count(), vs.choice(), rule.no_ast_opt);
    }

    if (collapse && vs.size() == 1) {
      const auto &child = std::any_cast<const std::shared_ptr<T> &>(vs[0]);

      // Unless something else holds the child (the packrat cache, a user
      // action), it stands in for this node itself instead of a copy of it.
      // It then keeps its Annotation, which a copy would reset.
      if (child.use_count() == 1) {
        if (!child->preserve_position) {
          child->position = position;
          child->length = length;
        }
        child->original_name = rule.ast_node_name;
        child->original_tag = rule.ast_node_tag;
        child->original_choice_count = vs.choice_count();
        child->original_choice = vs.choice();
        return child;
      }

      return collapse_ast_node(*child, node_name, position, length,
                               vs.choice_count(), vs.choice());
    }

    auto line = vs.line_info();

    // Construct with no children, then move the collected ones in: passing
    // the vector to the constructor would bind to its `const &` parameter
    // and copy the whole thing (plus a reference count bump per child).
    auto ast =
        std::make_shared<T>(vs.path, line.first, line.second, node_name,
                            std::vector<std::shared_ptr<T>>(), position, length,
                            vs.choice_count(), vs.choice(), rule.no_ast_opt);
    ast->nodes = vs.transform<std::shared_ptr<T>>();

    for (auto &node : ast->nodes) {
      node->parent = ast;
    }
    return ast;
  };
  rule.action.template declare_ast_action<std::shared_ptr<T>>(collapse);
}

#define PEG_EXPAND(...) __VA_ARGS__
#define PEG_CONCAT(a, b) a##b
#define PEG_CONCAT2(a, b) PEG_CONCAT(a, b)

#define PEG_PICK(                                                              \
    a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, \
    a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31, \
    a32, a33, a34, a35, a36, a37, a38, a39, a40, a41, a42, a43, a44, a45, a46, \
    a47, a48, a49, a50, a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, \
    a62, a63, a64, a65, a66, a67, a68, a69, a70, a71, a72, a73, a74, a75, a76, \
    a77, a78, a79, a80, a81, a82, a83, a84, a85, a86, a87, a88, a89, a90, a91, \
    a92, a93, a94, a95, a96, a97, a98, a99, a100, ...)                         \
  a100

#define PEG_COUNT(...)                                                         \
  PEG_EXPAND(PEG_PICK(                                                         \
      __VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87,    \
      86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69,  \
      68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51,  \
      50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33,  \
      32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,  \
      14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define PEG_DEF_1(r)                                                           \
  peg::Definition r;                                                           \
  r.name = #r;                                                                 \
  peg::add_ast_action(r);

#define PEG_DEF_2(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_1(__VA_ARGS__))
#define PEG_DEF_3(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_2(__VA_ARGS__))
#define PEG_DEF_4(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_3(__VA_ARGS__))
#define PEG_DEF_5(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_4(__VA_ARGS__))
#define PEG_DEF_6(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_5(__VA_ARGS__))
#define PEG_DEF_7(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_6(__VA_ARGS__))
#define PEG_DEF_8(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_7(__VA_ARGS__))
#define PEG_DEF_9(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_8(__VA_ARGS__))
#define PEG_DEF_10(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_9(__VA_ARGS__))
#define PEG_DEF_11(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_10(__VA_ARGS__))
#define PEG_DEF_12(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_11(__VA_ARGS__))
#define PEG_DEF_13(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_12(__VA_ARGS__))
#define PEG_DEF_14(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_13(__VA_ARGS__))
#define PEG_DEF_15(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_14(__VA_ARGS__))
#define PEG_DEF_16(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_15(__VA_ARGS__))
#define PEG_DEF_17(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_16(__VA_ARGS__))
#define PEG_DEF_18(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_17(__VA_ARGS__))
#define PEG_DEF_19(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_18(__VA_ARGS__))
#define PEG_DEF_20(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_19(__VA_ARGS__))
#define PEG_DEF_21(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_20(__VA_ARGS__))
#define PEG_DEF_22(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_21(__VA_ARGS__))
#define PEG_DEF_23(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_22(__VA_ARGS__))
#define PEG_DEF_24(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_23(__VA_ARGS__))
#define PEG_DEF_25(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_24(__VA_ARGS__))
#define PEG_DEF_26(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_25(__VA_ARGS__))
#define PEG_DEF_27(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_26(__VA_ARGS__))
#define PEG_DEF_28(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_27(__VA_ARGS__))
#define PEG_DEF_29(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_28(__VA_ARGS__))
#define PEG_DEF_30(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_29(__VA_ARGS__))
#define PEG_DEF_31(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_30(__VA_ARGS__))
#define PEG_DEF_32(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_31(__VA_ARGS__))
#define PEG_DEF_33(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_32(__VA_ARGS__))
#define PEG_DEF_34(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_33(__VA_ARGS__))
#define PEG_DEF_35(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_34(__VA_ARGS__))
#define PEG_DEF_36(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_35(__VA_ARGS__))
#define PEG_DEF_37(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_36(__VA_ARGS__))
#define PEG_DEF_38(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_37(__VA_ARGS__))
#define PEG_DEF_39(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_38(__VA_ARGS__))
#define PEG_DEF_40(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_39(__VA_ARGS__))
#define PEG_DEF_41(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_40(__VA_ARGS__))
#define PEG_DEF_42(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_41(__VA_ARGS__))
#define PEG_DEF_43(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_42(__VA_ARGS__))
#define PEG_DEF_44(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_43(__VA_ARGS__))
#define PEG_DEF_45(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_44(__VA_ARGS__))
#define PEG_DEF_46(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_45(__VA_ARGS__))
#define PEG_DEF_47(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_46(__VA_ARGS__))
#define PEG_DEF_48(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_47(__VA_ARGS__))
#define PEG_DEF_49(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_48(__VA_ARGS__))
#define PEG_DEF_50(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_49(__VA_ARGS__))
#define PEG_DEF_51(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_50(__VA_ARGS__))
#define PEG_DEF_52(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_51(__VA_ARGS__))
#define PEG_DEF_53(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_52(__VA_ARGS__))
#define PEG_DEF_54(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_53(__VA_ARGS__))
#define PEG_DEF_55(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_54(__VA_ARGS__))
#define PEG_DEF_56(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_55(__VA_ARGS__))
#define PEG_DEF_57(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_56(__VA_ARGS__))
#define PEG_DEF_58(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_57(__VA_ARGS__))
#define PEG_DEF_59(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_58(__VA_ARGS__))
#define PEG_DEF_60(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_59(__VA_ARGS__))
#define PEG_DEF_61(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_60(__VA_ARGS__))
#define PEG_DEF_62(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_61(__VA_ARGS__))
#define PEG_DEF_63(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_62(__VA_ARGS__))
#define PEG_DEF_64(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_63(__VA_ARGS__))
#define PEG_DEF_65(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_64(__VA_ARGS__))
#define PEG_DEF_66(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_65(__VA_ARGS__))
#define PEG_DEF_67(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_66(__VA_ARGS__))
#define PEG_DEF_68(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_67(__VA_ARGS__))
#define PEG_DEF_69(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_68(__VA_ARGS__))
#define PEG_DEF_70(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_69(__VA_ARGS__))
#define PEG_DEF_71(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_70(__VA_ARGS__))
#define PEG_DEF_72(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_71(__VA_ARGS__))
#define PEG_DEF_73(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_72(__VA_ARGS__))
#define PEG_DEF_74(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_73(__VA_ARGS__))
#define PEG_DEF_75(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_74(__VA_ARGS__))
#define PEG_DEF_76(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_75(__VA_ARGS__))
#define PEG_DEF_77(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_76(__VA_ARGS__))
#define PEG_DEF_78(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_77(__VA_ARGS__))
#define PEG_DEF_79(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_78(__VA_ARGS__))
#define PEG_DEF_80(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_79(__VA_ARGS__))
#define PEG_DEF_81(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_80(__VA_ARGS__))
#define PEG_DEF_82(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_81(__VA_ARGS__))
#define PEG_DEF_83(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_82(__VA_ARGS__))
#define PEG_DEF_84(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_83(__VA_ARGS__))
#define PEG_DEF_85(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_84(__VA_ARGS__))
#define PEG_DEF_86(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_85(__VA_ARGS__))
#define PEG_DEF_87(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_86(__VA_ARGS__))
#define PEG_DEF_88(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_87(__VA_ARGS__))
#define PEG_DEF_89(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_88(__VA_ARGS__))
#define PEG_DEF_90(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_89(__VA_ARGS__))
#define PEG_DEF_91(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_90(__VA_ARGS__))
#define PEG_DEF_92(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_91(__VA_ARGS__))
#define PEG_DEF_93(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_92(__VA_ARGS__))
#define PEG_DEF_94(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_93(__VA_ARGS__))
#define PEG_DEF_95(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_94(__VA_ARGS__))
#define PEG_DEF_96(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_95(__VA_ARGS__))
#define PEG_DEF_97(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_96(__VA_ARGS__))
#define PEG_DEF_98(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_97(__VA_ARGS__))
#define PEG_DEF_99(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_98(__VA_ARGS__))
#define PEG_DEF_100(r1, ...) PEG_EXPAND(PEG_DEF_1(r1) PEG_DEF_99(__VA_ARGS__))

#define AST_DEFINITIONS(...)                                                   \
  PEG_EXPAND(PEG_CONCAT2(PEG_DEF_, PEG_COUNT(__VA_ARGS__))(__VA_ARGS__))

/*-----------------------------------------------------------------------------
 *  parser
 *---------------------------------------------------------------------------*/

class parser {
public:
  parser() = default;

  parser(const char *s, size_t n, const Rules &rules,
         std::string_view start = {}) {
    load_grammar(s, n, rules, start);
  }

  parser(const char *s, size_t n, std::string_view start = {})
      : parser(s, n, Rules(), start) {}

  parser(std::string_view sv, const Rules &rules, std::string_view start = {})
      : parser(sv.data(), sv.size(), rules, start) {}

  parser(std::string_view sv, std::string_view start = {})
      : parser(sv.data(), sv.size(), Rules(), start) {}

#if defined(__cpp_lib_char8_t)
  parser(std::u8string_view sv, const Rules &rules, std::string_view start = {})
      : parser(u8(sv.data()), sv.size(), rules, start) {}

  parser(std::u8string_view sv, std::string_view start = {})
      : parser(u8(sv.data()), sv.size(), Rules(), start) {}
#endif

  operator bool() const { return grammar_ != nullptr; }

  bool load_grammar(const char *s, size_t n, const Rules &rules,
                    std::string_view start = {}) {
    auto cxt =
        ParserGenerator::parse(s, n, rules, log_, start, enableLeftRecursion_);
    grammar_ = cxt.grammar;
    start_ = cxt.start;
    enablePackratParsing_ = cxt.enablePackratParsing;
#ifdef CPPPEGLIB_TEST
    // For the test suite: replace the grammar with the one its blob loads, so
    // every test also checks GrammarBlob. A grammar that a blob cannot hold
    // stays as parsed. The environment variable lets one test binary run both
    // ways.
    if (grammar_ != nullptr && std::getenv("CPPPEGLIB_TEST_BLOB_ROUNDTRIP")) {
      std::vector<uint8_t> blob;
      try {
        blob = serialize_grammar();
      } catch (const std::runtime_error &) {}
      if (!blob.empty() && !load_blob(blob)) {
        throw std::logic_error("a serialized grammar failed to load");
      }
    }
#endif
    return grammar_ != nullptr;
  }

  bool load_grammar(const char *s, size_t n, std::string_view start = {}) {
    return load_grammar(s, n, Rules(), start);
  }

  bool load_grammar(std::string_view sv, const Rules &rules,
                    std::string_view start = {}) {
    return load_grammar(sv.data(), sv.size(), rules, start);
  }

  bool load_grammar(std::string_view sv, std::string_view start = {}) {
    return load_grammar(sv.data(), sv.size(), Rules(), start);
  }

  // Serialize the loaded grammar to a portable byte blob (see GrammarBlob).
  // Semantic callbacks are not included; throws if the grammar is not
  // serializable (uses the `User` operator or a Capture with a match action).
  // Without a loaded grammar the blob is empty, which load_blob() rejects.
  std::vector<uint8_t> serialize_grammar() const {
    if (grammar_ == nullptr) { return {}; }
    return GrammarBlob::serialize(*grammar_, start_);
  }

  // Load a grammar from a blob produced by serialize_grammar() / GrammarBlob,
  // skipping the meta-parse. Re-apply enable_ast() etc. afterwards as needed.
  bool load_blob(const std::vector<uint8_t> &blob) {
    try {
      grammar_ = GrammarBlob::deserialize(blob, start_);
    } catch (const std::exception &) { return false; }
    if (grammar_ != nullptr) {
      // enablePackratParsing_ says whether the grammar allows packrat, which
      // only a back reference to a capture in another rule forbids. Such a
      // grammar holds a capture, which a blob cannot, so a blob always allows
      // packrat. Whether packrat is on is the start rule's own flag, which the
      // blob restores.
      enablePackratParsing_ = true;
    }
    return grammar_ != nullptr;
  }

  bool parse_n(const char *s, size_t n, const char *path = nullptr) const {
    return parse_from_start([&](const Definition &rule) {
      auto result = rule.parse(s, n, path, log_, error_reporter_);
      return post_process(s, n, result);
    });
  }

  bool parse_n(const char *s, size_t n, std::any &dt,
               const char *path = nullptr) const {
    return parse_from_start([&](const Definition &rule) {
      auto result = rule.parse(s, n, dt, path, log_, error_reporter_);
      return post_process(s, n, result);
    });
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, T &val,
               const char *path = nullptr) const {
    return parse_from_start([&](const Definition &rule) {
      auto result =
          rule.parse_and_get_value(s, n, val, path, log_, error_reporter_);
      return post_process(rule, s, n, val, result);
    });
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, std::any &dt, T &val,
               const char *path = nullptr) const {
    return parse_from_start([&](const Definition &rule) {
      auto result =
          rule.parse_and_get_value(s, n, dt, val, path, log_, error_reporter_);
      return post_process(rule, s, n, val, result);
    });
  }

  bool parse(std::string_view sv, const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), path);
  }

  bool parse(std::string_view sv, std::any &dt,
             const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), dt, path);
  }

  template <typename T>
  bool parse(std::string_view sv, T &val, const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), val, path);
  }

  template <typename T>
  bool parse(std::string_view sv, std::any &dt, T &val,
             const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), dt, val, path);
  }

#if defined(__cpp_lib_char8_t)
  bool parse(std::u8string_view sv, const char *path = nullptr) const {
    return parse(as_chars(sv), path);
  }

  bool parse(std::u8string_view sv, std::any &dt,
             const char *path = nullptr) const {
    return parse(as_chars(sv), dt, path);
  }

  template <typename T>
  bool parse(std::u8string_view sv, T &val, const char *path = nullptr) const {
    return parse(as_chars(sv), val, path);
  }

  template <typename T>
  bool parse(std::u8string_view sv, std::any &dt, T &val,
             const char *path = nullptr) const {
    return parse(as_chars(sv), dt, val, path);
  }
#endif

  Definition &operator[](const char *s) { return (*grammar_)[s]; }

  const Definition &operator[](const char *s) const { return (*grammar_)[s]; }

  const Grammar &get_grammar() const { return *grammar_; }

  void disable_eoi_check() {
    with_start_rule([](Definition &rule) { rule.eoi_check = false; });
  }

  // Applies to the next load_grammar, not to a grammar already loaded.
  void enable_left_recursion(bool enable = true) {
    enableLeftRecursion_ = enable;
  }

  void enable_packrat_parsing() {
    with_start_rule([this](Definition &rule) {
      rule.enablePackratParsing = enablePackratParsing_;
    });
  }

  void enable_trace(TracerEnter tracer_enter, TracerLeave tracer_leave) {
    with_start_rule([&](Definition &rule) {
      rule.tracer_enter = tracer_enter;
      rule.tracer_leave = tracer_leave;
    });
  }

  void enable_trace(TracerEnter tracer_enter, TracerLeave tracer_leave,
                    TracerStartOrEnd tracer_start,
                    TracerStartOrEnd tracer_end) {
    with_start_rule([&](Definition &rule) {
      rule.tracer_enter = tracer_enter;
      rule.tracer_leave = tracer_leave;
      rule.tracer_start = tracer_start;
      rule.tracer_end = tracer_end;
    });
  }

  void set_verbose_trace(bool verbose_trace) {
    with_start_rule(
        [&](Definition &rule) { rule.verbose_trace = verbose_trace; });
  }

  // Fails a parse that nests more than max_depth rule matches, with an
  // error, before deep input can overflow the stack, and one that returns
  // an AST deeper than that (see check_ast_depth).
  void set_max_depth(size_t max_depth) {
    with_start_rule([&](Definition &rule) { rule.max_depth = max_depth; });
  }

  // With collapse_mode, nodes are collapsed as they are built, giving the
  // tree optimize_ast(ast, opt_mode) would return without copying it again.
  // Rules with a user action are not collapsed.
  template <typename T = Ast>
  parser &enable_ast(bool collapse_mode = false, bool opt_mode = true) {
    return enable_ast<T>(collapse_mode, opt_mode, get_no_ast_opt_rules());
  }

  // As above, but with `rules` in place of the rules marked no_ast_opt, as
  // in AstOptimizer(opt_mode, rules).
  template <typename T = Ast>
  parser &enable_ast(bool collapse_mode, bool opt_mode,
                     const std::vector<std::string> &rules) {
    if (grammar_ == nullptr) { return *this; }
    const AstOptimizer optimizer(opt_mode, rules);
    for (auto &[_, rule] : *grammar_) {
      if (!rule.action) {
        add_ast_action<T>(rule, collapse_mode &&
                                    optimizer.is_optimized(rule.node_name()));
      }
    }
    builds_ast_ = true;
    return *this;
  }

  template <typename T>
  std::shared_ptr<T> optimize_ast(std::shared_ptr<T> ast,
                                  bool opt_mode = true) const {
    return AstOptimizer(opt_mode, get_no_ast_opt_rules()).optimize(ast);
  }

  void set_logger(Log log) { log_ = log; }

  // Receive structured error information instead of (or in addition to) the
  // formatted string passed to the logger.
  void set_error_reporter(ErrorReporter reporter) {
    error_reporter_ = reporter;
  }

  void set_logger(
      std::function<void(size_t line, size_t col, const std::string &msg)>
          log) {
    log_ = [log](size_t line, size_t col, const std::string &msg,
                 const std::string & /*rule*/) { log(line, col, msg); };
  }

private:
  // Runs `f` on the start rule, if a grammar is loaded.
  template <typename F> void with_start_rule(F f) {
    if (grammar_ != nullptr) { f((*grammar_)[start_]); }
  }

  // Runs `parse` on the start rule, which fails without a grammar.
  template <typename F> bool parse_from_start(F parse) const {
    return grammar_ != nullptr && parse((*grammar_)[start_]);
  }

#if defined(__cpp_lib_char8_t)
  static std::string_view as_chars(std::u8string_view sv) {
    return {u8(sv.data()), sv.size()};
  }
#endif

  bool post_process(const char *s, size_t n, Definition::Result &r) const {
    if ((log_ || error_reporter_) && !r.ret) {
      r.error_info.output_log(log_, error_reporter_, s, n);
    }
    return r.ret && !r.recovered;
  }

  template <typename T>
  bool post_process(const Definition &rule, const char *s, size_t n, T &val,
                    Definition::Result &r) const {
    check_ast_depth(rule, s, n, val, r);
    auto ret = post_process(s, n, r);
    if (ret) { link_ast(val); }
    return ret;
  }

  // A tree can nest deeper than its parse did: a left-associative precedence
  // operator or left recursion folds a chain in a loop, and packrat parsing
  // reuses a subtree wherever it matches again. Under a max_depth, the
  // returned tree is held to it too, and one that goes past it is reported at
  // its first node that does, as the parse reports the rule match that does.
  template <typename V>
  void check_ast_depth(const Definition &, const char *, size_t, V &,
                       Definition::Result &) const {}
  template <typename A>
  void check_ast_depth(const Definition &rule, const char *s, size_t n,
                       std::shared_ptr<AstBase<A>> &ast,
                       Definition::Result &r) const {
    if (!r.ret || !ast ||
        rule.max_depth == std::numeric_limits<size_t>::max()) {
      return;
    }
    std::vector<std::pair<const AstBase<A> *, size_t>> stack{{ast.get(), 1}};
    while (!stack.empty()) {
      auto [node, depth] = stack.back();
      stack.pop_back();
      if (depth > rule.max_depth) {
        r.ret = false;
        // A node a user action took from another parse can start past the
        // end of this input.
        r.error_info.set_nesting_too_deep(s + std::min(node->position, n),
                                          node->name, rule.max_depth);
        ast.reset();
        return;
      }
      // In reverse, so that nodes are visited in the order of the text.
      for (auto it = node->nodes.rbegin(); it != node->nodes.rend(); ++it) {
        stack.emplace_back(it->get(), depth + 1);
      }
    }
  }

  // Packrat or left recursion can reuse a node after a parent that took it in
  // was discarded, leaving it with a stale parent, so link the finished tree
  // again. A direct Definition::parse_and_get_value call skips this.
  template <typename V> void link_ast(V &) const {}
  template <typename A> void link_ast(std::shared_ptr<AstBase<A>> &ast) const {
    if (!builds_ast_ || !ast) { return; }
    ast->parent.reset();
    // Only a zero-length node can sit under two parents (packrat reuses it);
    // each extra occurrence gets its own copy.
    std::unordered_set<const AstBase<A> *> zero_length;
    std::vector<std::shared_ptr<AstBase<A>> *> stack{&ast};
    while (!stack.empty()) {
      auto &node = *stack.back();
      stack.pop_back();
      for (auto &child : node->nodes) {
        if (child->length == 0 && !zero_length.insert(child.get()).second) {
          child = std::make_shared<AstBase<A>>(*child);
          zero_length.insert(child.get());
        }
        child->parent = node;
        stack.push_back(&child);
      }
    }
  }

  std::vector<std::string> get_no_ast_opt_rules() const {
    std::vector<std::string> rules;
    if (grammar_ == nullptr) { return rules; }
    for (auto &[_, rule] : *grammar_) {
      // The optimizer keeps nodes by their emitted name, so honor the
      // `ast_name` override when present (else the rule's own name).
      if (rule.no_ast_opt) { rules.push_back(rule.node_name()); }
    }
    return rules;
  }

  std::shared_ptr<Grammar> grammar_;
  std::string start_;
  bool enableLeftRecursion_ = true;
  bool enablePackratParsing_ = false;
  bool builds_ast_ = false; // enable_ast() was called
  Log log_;
  ErrorReporter error_reporter_;
};

/*-----------------------------------------------------------------------------
 *  enable_tracing
 *---------------------------------------------------------------------------*/

// The bars that show how deep in the trace a rule match is.
inline std::string trace_indent(const Context &c) {
  std::string indent;
  for (size_t level = 1; level < c.trace_ids.size(); level++) {
    indent += "│";
  }
  return indent;
}

inline void enable_tracing(parser &parser, std::ostream &os) {
  parser.enable_trace(
      [&](auto &ope, auto s, auto, auto &, auto &c, auto &, auto &trace_data) {
        auto prev_pos = std::any_cast<size_t>(trace_data);
        auto pos = static_cast<size_t>(s - c.s);
        auto backtrack = (pos < prev_pos ? "*" : "");
        auto name = peg::TraceOpeName::get(ope);
        if (auto lit = dynamic_cast<const peg::LiteralString *>(&ope)) {
          name += " '" + peg::escape_characters(lit->lit_) + "'";
        }
        os << "E " << pos + 1 << backtrack << "\t" << trace_indent(c) << "┌"
           << name << " #" << c.trace_ids.back() << std::endl;
        trace_data = static_cast<size_t>(pos);
      },
      [&](auto &ope, auto s, auto, auto &sv, auto &c, auto &, auto len,
          auto &) {
        auto pos = static_cast<size_t>(s - c.s);
        if (peg::success(len)) { pos += len; }
        auto ret = peg::success(len) ? "└o " : "└x ";
        auto name = peg::TraceOpeName::get(ope);
        std::stringstream choice;
        if (sv.choice_count() > 0) {
          choice << " " << sv.choice() << "/" << sv.choice_count();
        }
        std::string token;
        if (!sv.tokens.empty()) {
          token += ", token '";
          token += sv.tokens[0];
          token += "'";
        }
        std::string matched;
        if (peg::success(len) &&
            peg::TokenChecker::is_token(const_cast<peg::Ope &>(ope))) {
          matched = ", match '" + peg::escape_characters(s, len) + "'";
        }
        os << "L " << pos + 1 << "\t" << trace_indent(c) << ret << name << " #"
           << c.trace_ids.back() << choice.str() << token << matched
           << std::endl;
      },
      [&](auto &trace_data) { trace_data = static_cast<size_t>(0); },
      [&](auto &) {});
}

/*-----------------------------------------------------------------------------
 *  enable_profiling
 *---------------------------------------------------------------------------*/

inline void enable_profiling(parser &parser, std::ostream &os) {
  struct Stats {
    struct Item {
      std::string name;
      size_t success;
      size_t fail;
    };
    std::vector<Item> items;
    std::map<std::string, size_t> index;
    size_t total = 0;
    std::chrono::steady_clock::time_point start;
  };

  parser.enable_trace(
      [&](auto &ope, auto, auto, auto &, auto &, auto &, std::any &trace_data) {
        if (auto holder = dynamic_cast<const peg::Holder *>(&ope)) {
          auto &stats = *std::any_cast<std::shared_ptr<Stats> &>(trace_data);

          auto &name = holder->name();
          if (stats.index.emplace(name, stats.items.size()).second) {
            stats.items.push_back({name, 0, 0});
          }
          stats.total++;
        }
      },
      [&](auto &ope, auto, auto, auto &, auto &, auto &, auto len,
          std::any &trace_data) {
        if (auto holder = dynamic_cast<const peg::Holder *>(&ope)) {
          auto &stats = *std::any_cast<std::shared_ptr<Stats> &>(trace_data);

          auto &stat = stats.items[stats.index[holder->name()]];
          if (peg::success(len)) {
            stat.success++;
          } else {
            stat.fail++;
          }
        }
      },
      [&](auto &trace_data) {
        auto stats = std::make_shared<Stats>();
        stats->start = std::chrono::steady_clock::now();
        trace_data = std::move(stats);
      },
      // Reports here, where every parse ends, rather than where the start
      // rule is left: a parse that an exception cuts short never gets there,
      // and a start rule that matches inside itself gets there many times.
      [&](auto &trace_data) {
        auto &stats = *std::any_cast<std::shared_ptr<Stats> &>(trace_data);

        auto end = std::chrono::steady_clock::now();
        auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
                        end - stats.start)
                        .count();
        auto sec = usec / 1000000.0;
        os << "duration: " << sec << "s (" << usec << "µs)" << std::endl
           << std::endl;

        char buff[BUFSIZ];
        size_t total_success = 0;
        size_t total_fail = 0;
        for (auto &[name, success, fail] : stats.items) {
          total_success += success;
          total_fail += fail;
        }

        os << "  id       total      %     success        fail  "
              "definition"
           << std::endl;

        auto grand_total = total_success + total_fail;
        snprintf(buff, BUFSIZ, "%4s  %10zu  %5s  %10zu  %10zu  %s", "",
                 grand_total, "", total_success, total_fail, "Total counters");
        os << buff << std::endl;

        // A parse cut short may have left no rule at all.
        auto percent = [&](size_t count) {
          return grand_total ? count * 100.0 / grand_total : 0.0;
        };
        snprintf(buff, BUFSIZ, "%4s  %10s  %5s  %10.2f  %10.2f  %s", "", "", "",
                 percent(total_success), percent(total_fail), "% success/fail");
        os << buff << std::endl << std::endl;

        size_t id = 0;
        for (auto &[name, success, fail] : stats.items) {
          auto total = success + fail;
          auto ratio = total * 100.0 / stats.total;
          snprintf(buff, BUFSIZ, "%4zu  %10zu  %5.2f  %10zu  %10zu  %s", id,
                   total, ratio, success, fail, name.c_str());
          os << buff << std::endl;
          id++;
        }
      });
}
} // namespace peg
