#ifndef PARSER_LIST_TREE_AST_HPP
#define PARSER_LIST_TREE_AST_HPP

#include <memory>
#include <variant>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/tokens/token.hpp"

namespace parsers::list_tree {

class Expr;

class TokenExpr {
 public:
  TokenExpr(tokens::Token token);

  tokens::Token const& token() const;

  void WriteTokens(std::vector<tokens::Token>* tokens) const;

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
  enum Kind {
    PARENS,
    BRACKETS,
  };

  ListExpr(Kind kind, tokens::Token open_token, tokens::Token close_token,
           std::vector<Expr> elements);

  Kind kind() const;
  tokens::Token const& open_token() const;
  tokens::Token const& close_token() const;

  absl::Span<Expr const> elements() const;

  void WriteTokens(std::vector<tokens::Token>* tokens) const;

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
  void WriteTokens(std::vector<tokens::Token>* tokens) const;

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

}  // namespace parsers::list_tree

#endif