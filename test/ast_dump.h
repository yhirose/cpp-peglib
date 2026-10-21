#pragma once

// Dumps of AST nodes, for the tests that compare what callbacks see of the
// nodes and the tree a parse returns.

#include <peglib.h>

#include <map>
#include <memory>
#include <string>

// Numbers nodes in the order they are first seen, so the same node seen twice
// (or seen in a callback and then found in the tree) shows up with the same
// number. Nodes are told apart by owner: the number keeps the node's control
// block, so a node that dies leaves its number to no other node.
template <typename T> struct NodeIds {
  std::map<std::weak_ptr<T>, size_t, std::owner_less<>> ids;

  size_t id(const std::shared_ptr<T> &node) {
    return ids.try_emplace(std::weak_ptr<T>(node), ids.size() + 1)
        .first->second;
  }
};

// Every field of `node` and of the nodes below it, a line each, indented by
// depth. With `check_parents`, a child whose parent is not the node it is
// under is marked: a node's parent is promised only in the tree a parse
// returns, once it has returned.
template <typename T>
void dump_ast(const std::shared_ptr<T> &node, NodeIds<T> &ids, std::string &out,
              bool check_parents, size_t depth = 0) {
  out += std::string(depth * 2, ' ') + "#" + std::to_string(ids.id(node)) +
         " " + node->name + "|" + node->original_name + "|" +
         std::to_string(node->position) + "+" + std::to_string(node->length) +
         "|" + std::to_string(node->line) + ":" + std::to_string(node->column) +
         "|" + std::to_string(node->choice_count) + "/" +
         std::to_string(node->choice) + "|" +
         std::to_string(node->original_choice_count) + "/" +
         std::to_string(node->original_choice) + "|" +
         std::to_string(node->tag) + "/" + std::to_string(node->original_tag) +
         "|" + (node->is_token ? "'" + std::string(node->token) + "'" : "-") +
         "|" + node->path + "|" + std::to_string(node->preserve_position) +
         "\n";
  for (const auto &child : node->nodes) {
    if (check_parents && child->parent.lock() != node) {
      out += std::string(depth * 2 + 2, ' ') + "WRONG PARENT\n";
    }
    dump_ast(child, ids, out, check_parents, depth + 1);
  }
}
