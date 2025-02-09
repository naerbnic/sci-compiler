#include "scic/parsers/list_tree/ast.hpp"

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "absl/types/span.h"
#include "scic/tokenizer/token.hpp"

namespace parser::list_tree {

using ::tokenizer::Token;

struct TokenExpr::PImpl {
  Token token;
};

TokenExpr::TokenExpr(Token token)
    : pimpl_(std::make_shared<PImpl>(PImpl{
          .token = std::move(token),
      })) {}

Token const& TokenExpr::token() const { return pimpl_->token; }

struct ListExpr::PImpl {
  std::vector<Expr> elements;
  Token start_paren;
  Token end_paren;
};

ListExpr::ListExpr(Token start_paren, Token end_paren,
                   std::vector<Expr> elements)
    : pimpl_(std::make_shared<PImpl>(PImpl{
          .elements = std::move(elements),
      })) {}

Token const& ListExpr::start_paren() const { return pimpl_->start_paren; }
Token const& ListExpr::end_paren() const { return pimpl_->end_paren; }

absl::Span<Expr const> ListExpr::elements() const {
  return absl::MakeConstSpan(pimpl_->elements);
}

Expr::Expr(TokenExpr token_expr) : expr_(std::move(token_expr)) {}
Expr::Expr(ListExpr list_expr) : expr_(std::move(list_expr)) {}

TokenExpr const* Expr::AsTokenExpr() const {
  return std::get_if<TokenExpr>(&expr_);
}

ListExpr const* Expr::AsListExpr() const {
  return std::get_if<ListExpr>(&expr_);
}

}  // namespace parser::list_tree