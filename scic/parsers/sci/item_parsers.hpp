#ifndef PARSERS_SCI_ITEM_PARSERS_HPP
#define PARSERS_SCI_ITEM_PARSERS_HPP

#include <string_view>

#include "scic/parsers/combinators/parse_func.hpp"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"

namespace parsers::sci {

using ItemParser =
    ParseFunc<Item(TokenNode<std::string_view> const&, TreeExprSpan&)>;

ParseResult<Item> ParsePublicItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs);
ParseResult<Item> ParseExternItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs);
ParseResult<Item> ParseGlobalDeclItem(
    TokenNode<std::string_view> const& keyword, TreeExprSpan& exprs);
ParseResult<Item> ParseGlobalItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs);
ParseResult<Item> ParseLocalItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs);
ParseResult<Item> ParseProcItem(TokenNode<std::string_view> const& keyword,
                                TreeExprSpan& exprs);
ParseResult<Item> ParseClassItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs);
ParseResult<Item> ParseInstanceItem(TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs);
ParseResult<Item> ParseClassDeclItem(TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs);
ParseResult<Item> ParseSelectorsItem(TokenNode<std::string_view> const& keyword,
                                     TreeExprSpan& exprs);

ParseResult<Item> ParseItem(TreeExprSpan& exprs);

}  // namespace parsers::sci

#endif