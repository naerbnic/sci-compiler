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

absl::StatusOr<std::vector<Expr>> ParseListTree(
    std::vector<tokens::Token> initial_tokens,
    IncludeContext const* include_context,
    absl::btree_map<std::string, std::vector<tokens::Token>> const&
        initial_defines);

}  // namespace parser::list_tree

#endif