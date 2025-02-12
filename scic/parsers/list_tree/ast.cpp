#include "scic/parsers/list_tree/ast.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "scic/tokens/token.hpp"

namespace parsers::list_tree {

using ::tokens::Token;

struct TokenExpr::PImpl {
  Token token;
};

TokenExpr::TokenExpr(Token token)
    : pimpl_(std::make_shared<PImpl>(PImpl{
          .token = std::move(token),
      })) {}

Token const& TokenExpr::token() const { return pimpl_->token; }

void TokenExpr::WriteTokens(std::vector<tokens::Token>* tokens) const {
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

void ListExpr::WriteTokens(std::vector<tokens::Token>* tokens) const {
  tokens->push_back(pimpl_->open_token);
  for (auto const& element : pimpl_->elements) {
    element.WriteTokens(tokens);
  }
  tokens->push_back(pimpl_->close_token);
}

void Expr::WriteTokens(std::vector<tokens::Token>* tokens) const {
  visit([&](auto const& expr) { expr.WriteTokens(tokens); });
}

}  // namespace parsers::list_tree