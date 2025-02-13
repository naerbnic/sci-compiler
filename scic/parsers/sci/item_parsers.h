#ifndef PARSERS_SCI_ITEM_PARSERS_HPP
#define PARSERS_SCI_ITEM_PARSERS_HPP

#include <string_view>

#include "absl/types/span.h"
#include "scic/parsers/combinators/parse_func.hpp"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"

namespace parsers::sci {

using ItemParser = ParseFunc<Item(TokenNode<std::string_view> const&,
                                  absl::Span<list_tree::Expr const>&)>;

ParseResult<Item> ParsePublicItem(TokenNode<std::string_view> const& keyword,
                                  absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseExternItem(TokenNode<std::string_view> const& keyword,
                                  absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseGlobalDeclItem(
    TokenNode<std::string_view> const& keyword,
    absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseGlobalItem(TokenNode<std::string_view> const& keyword,
                                  absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseLocalItem(TokenNode<std::string_view> const& keyword,
                                 absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseProcItem(TokenNode<std::string_view> const& keyword,
                                absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseClassItem(TokenNode<std::string_view> const& keyword,
                                 absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseInstanceItem(TokenNode<std::string_view> const& keyword,
                                    absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseClassDefItem(TokenNode<std::string_view> const& keyword,
                                    absl::Span<list_tree::Expr const>& exprs);
ParseResult<Item> ParseSelectorsItem(TokenNode<std::string_view> const& keyword,
                                     absl::Span<list_tree::Expr const>& exprs);

ParseResult<Item> ParseItem(absl::Span<list_tree::Expr const>& exprs);

}  // namespace parsers::sci

#endif