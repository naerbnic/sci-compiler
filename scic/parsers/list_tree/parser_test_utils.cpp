#include "scic/parsers/list_tree/parser_test_utils.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token_readers.hpp"

namespace parsers::list_tree {

std::vector<Expr> ParseExprsOrDie(std::string_view text) {
  auto tokens =
      tokens::TokenizeText(text::TextRange::OfString(std::string(text)))
          .value();
  Parser parser(IncludeContext::GetEmpty());
  return parser.ParseTree(std::move(tokens)).value();
}

Expr ParseExprOrDie(std::string_view text) {
  auto exprs = ParseExprsOrDie(text);
  if (exprs.size() != 1) {
    throw std::runtime_error("Expected a single expression.");
  }
  return std::move(exprs[0]);
}

}  // namespace parsers::list_tree
