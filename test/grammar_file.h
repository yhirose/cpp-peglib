#pragma once

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

// The text of a grammar in grammar/, whatever directory the tests run in.
inline std::string read_grammar(const std::string &name) {
  auto path = std::string(GRAMMAR_DIR) + name;
  std::ifstream ifs(path, std::ios::in | std::ios::binary);
  if (!ifs) { throw std::runtime_error("cannot open " + path); }
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}
