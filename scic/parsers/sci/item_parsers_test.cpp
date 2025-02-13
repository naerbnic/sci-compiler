#include "scic/parsers/sci/item_parsers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/list_tree/parser.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_readers.hpp"

namespace parsers::sci {
namespace {

class DummyIncludeContext : public list_tree::IncludeContext {
 public:
  static list_tree::IncludeContext const* Get() {
    static DummyIncludeContext context;
    return &context;
  };
  absl::StatusOr<std::vector<tokens::Token>> LoadTokensFromInclude(
      std::string_view path) const override {
    return absl::InvalidArgumentError("Not implemented");
  }
};

std::vector<list_tree::Expr> Preparse(std::string_view input) {
  auto tokens = tokens::TokenizeText(
      text::TextRange::WithFilename("test", std::string(input)));
  list_tree::Parser parser(DummyIncludeContext::Get());
  return parser.ParseTree(std::move(tokens).value()).value();
}

TokenNode<std::string_view> CreateDummyNode(std::string_view value) {
  return TokenNode<std::string_view>(value, text::TextRange::OfString(""));
}

TEST(PublicTest, Unimplemented) {
  auto exprs = Preparse("a b c");
  auto expr_span = absl::MakeConstSpan(exprs);
  auto result = ParsePublicItem(CreateDummyNode("public"), expr_span);
  EXPECT_FALSE(result.ok());
}

}  // namespace

}  // namespace parsers::sci