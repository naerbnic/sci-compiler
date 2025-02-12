#ifndef PARSER_LIST_TREE_PARSER_HPP
#define PARSER_LIST_TREE_PARSER_HPP

#include <string>
#include <string_view>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/status/statusor.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/tokens/token.hpp"

namespace parsers::list_tree {

class IncludeContext {
 public:
  virtual ~IncludeContext() = default;

  virtual absl::StatusOr<std::vector<tokens::Token>> LoadTokensFromInclude(
      std::string_view path) const = 0;
};

class Parser {
 public:
  Parser(IncludeContext const* include_context)
      : include_context_(include_context) {}

  // Add a define to the current context. Any instances of a simple identifier
  // with the given name will be substituted with the tokens provided.
  void AddDefine(std::string_view name, std::vector<tokens::Token> tokens);

  absl::StatusOr<std::vector<Expr>> ParseTree(
      std::vector<tokens::Token> tokens);

 private:
  IncludeContext const* include_context_;
  absl::btree_map<std::string, std::vector<tokens::Token>> defines_;
};

}  // namespace parsers::list_tree

#endif