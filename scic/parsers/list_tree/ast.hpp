#ifndef PARSER_LIST_TREE_AST_HPP
#define PARSER_LIST_TREE_AST_HPP

#include <memory>
#include <variant>
#include <vector>

#include "absl/types/span.h"
#include "scic/tokenizer/token.hpp"

namespace parser::list_tree {

class Expr;

class TokenExpr {
 public:
  TokenExpr(tokenizer::Token token);

  tokenizer::Token const& token() const;

 private:
  struct PImpl;
  std::shared_ptr<PImpl> pimpl_;
};

class ListExpr {
 public:
  ListExpr(tokenizer::Token start_paren, tokenizer::Token end_paren,
           std::vector<Expr> elements);

  tokenizer::Token const& start_paren() const;
  tokenizer::Token const& end_paren() const;

  absl::Span<Expr const> elements() const;

 private:
  struct PImpl;
  std::shared_ptr<PImpl> pimpl_;
};

class Expr {
 public:
  explicit Expr(TokenExpr token_expr);
  explicit Expr(ListExpr list_expr);

  TokenExpr const* AsTokenExpr() const;
  ListExpr const* AsListExpr() const;

 private:
  std::variant<TokenExpr, ListExpr> expr_;
};

}  // namespace parser::list_tree

#endif