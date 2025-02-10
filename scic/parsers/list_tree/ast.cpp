#include "scic/parsers/list_tree/ast.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "absl/types/span.h"
#include "scic/tokenizer/token.hpp"

namespace parsers::list_tree {

using ::tokenizer::Token;

struct TokenExpr::PImpl {
  Token token;
};

TokenExpr::TokenExpr(Token token)
    : pimpl_(std::make_shared<PImpl>(PImpl{
          .token = std::move(token),
      })) {}

Token const& TokenExpr::token() const { return pimpl_->token; }

void TokenExpr::WriteTokens(std::vector<tokenizer::Token>* tokens) const {
  tokens->push_back(pimpl_->token);
}

struct ListExpr::PImpl {
  Kind kind;
  std::vector<Expr> elements;
  Token open_token;
  Token close_token;
};

ListExpr::ListExpr(Kind kind, Token open_token, Token close_token,
                   std::vector<Expr> elements)
    : pimpl_(std::make_shared<PImpl>(PImpl{
          .kind = kind,
          .elements = std::move(elements),
          .open_token = std::move(open_token),
          .close_token = std::move(close_token),
      })) {}

Token const& ListExpr::open_token() const { return pimpl_->open_token; }
Token const& ListExpr::close_token() const { return pimpl_->close_token; }

absl::Span<Expr const> ListExpr::elements() const {
  return absl::MakeConstSpan(pimpl_->elements);
}

void ListExpr::WriteTokens(std::vector<tokenizer::Token>* tokens) const {
  tokens->push_back(pimpl_->open_token);
  for (auto const& element : pimpl_->elements) {
    element.WriteTokens(tokens);
  }
  tokens->push_back(pimpl_->close_token);
}

Expr::Expr(TokenExpr token_expr) : expr_(std::move(token_expr)) {}
Expr::Expr(ListExpr list_expr) : expr_(std::move(list_expr)) {}

TokenExpr const* Expr::AsTokenExpr() const {
  return std::get_if<TokenExpr>(&expr_);
}

ListExpr const* Expr::AsListExpr() const {
  return std::get_if<ListExpr>(&expr_);
}

void Expr::WriteTokens(std::vector<tokenizer::Token>* tokens) const {
  if (auto* token_expr = AsTokenExpr()) {
    token_expr->WriteTokens(tokens);
  } else if (auto* list_expr = AsListExpr()) {
    list_expr->WriteTokens(tokens);
  } else {
    throw std::runtime_error("Invalid expression type");
  }
}

}  // namespace parsers::list_tree