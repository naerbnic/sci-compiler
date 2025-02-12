#ifndef PARSER_LIST_TREE_AST_HPP
#define PARSER_LIST_TREE_AST_HPP

#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "util/choice.hpp"

namespace parsers::list_tree {

class Expr;

class TokenExpr {
 public:
  TokenExpr(tokens::Token token);

  tokens::Token const& token() const;
  text::TextRange const& text_range() const { return token().text_range(); }

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

class Expr : public util::ChoiceBase<Expr, TokenExpr, ListExpr> {
  using ChoiceBase::ChoiceBase;

 public:
  void WriteTokens(std::vector<tokens::Token>* tokens) const;

 private:
  template <class Sink>
  friend void AbslStringify(Sink& sink, Expr const& expr) {
    expr.visit(
        [&sink](ListExpr const& expr) { absl::Format(&sink, "%v", expr); },
        [&sink](TokenExpr const& expr) { absl::Format(&sink, "%v", expr); });
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