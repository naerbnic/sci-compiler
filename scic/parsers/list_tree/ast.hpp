#ifndef PARSER_LIST_TREE_AST_HPP
#define PARSER_LIST_TREE_AST_HPP

#include <memory>
#include <variant>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/tokenizer/token.hpp"

namespace parser::list_tree {

class Expr;

class TokenExpr {
 public:
  TokenExpr(tokenizer::Token token);

  tokenizer::Token const& token() const;

  void WriteTokens(std::vector<tokenizer::Token>* tokens) const;

 private:
  struct PImpl;
  std::shared_ptr<PImpl> pimpl_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, TokenExpr const& expr) {
    absl::Format(&sink, "%v", expr.token());
  }
};

class ListExpr {
 public:
  ListExpr(tokenizer::Token start_paren, tokenizer::Token end_paren,
           std::vector<Expr> elements);

  tokenizer::Token const& start_paren() const;
  tokenizer::Token const& end_paren() const;

  absl::Span<Expr const> elements() const;

  void WriteTokens(std::vector<tokenizer::Token>* tokens) const;

 private:
  struct PImpl;
  std::shared_ptr<PImpl> pimpl_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, ListExpr const& expr);
};

class Expr {
 public:
  explicit Expr(TokenExpr token_expr);
  explicit Expr(ListExpr list_expr);

  TokenExpr const* AsTokenExpr() const;
  ListExpr const* AsListExpr() const;

  // Write these tokens back in order to the given vector.
  void WriteTokens(std::vector<tokenizer::Token>* tokens) const;

 private:
  std::variant<TokenExpr, ListExpr> expr_;

  template <class Sink>
  friend void AbslStringify(Sink& sink, Expr const& expr) {
    if (auto* token_expr = expr.AsTokenExpr()) {
      absl::Format(&sink, "%v", *token_expr);
    } else if (auto* list_expr = expr.AsListExpr()) {
      absl::Format(&sink, "%v", *list_expr);
    }
  }
};

template <class Sink>
void AbslStringify(Sink& sink, ListExpr const& expr) {
  sink.Append("List(");
  bool first = true;
  for (auto const& element : expr.elements()) {
    if (!first) {
      absl::Format(&sink, ", ");
    }
    absl::Format(&sink, "%v", element);
    first = false;
  }
  sink.Append(")");
}

}  // namespace parser::list_tree

#endif