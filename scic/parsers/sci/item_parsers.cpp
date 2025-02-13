#include "scic/parsers/sci/item_parsers.hpp"

#include <map>
#include <string>
#include <string_view>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::sci {

ParseResult<Item> UnimplementedParseItem(
    TokenNode<std::string_view> const& keyword,
    absl::Span<list_tree::Expr const>& exprs) {
  return FailureOf("Unimplemented keyword: %s (%s)", keyword.value(),
                   keyword.text_range().GetRange().filename());
}

using ParserItemMap = std::map<std::string, ItemParser>;

ParserItemMap const& TopLevelParsers() {
  static ParserItemMap* instance = new ParserItemMap(([] {
    return ParserItemMap({
        {"public", ParsePublicItem},
        {"extern", ParseExternItem},
        {"global_decl", ParseGlobalDeclItem},
        {"global", ParseGlobalItem},
        {"local", ParseLocalItem},
        {"proc", ParseProcItem},
        {"class", ParseClassItem},
        {"instance", ParseInstanceItem},
        {"class_def", ParseClassDefItem},
        {"selectors", ParseSelectorsItem},
    });
  })());
  return *instance;
}

ItemParser const& GetItemParser(std::string_view keyword) {
  auto const& parsers = TopLevelParsers();
  auto it = parsers.find(std::string(keyword));
  if (it == parsers.end()) {
    static ItemParser const unimplemented_parser(UnimplementedParseItem);
    return unimplemented_parser;
  }
  return it->second;
}

ParseResult<Item> ParsePublicItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseExternItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseGlobalDeclItem(
    TokenNode<std::string_view> const& keyword, TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseGlobalItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseLocalItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseProcItem(TokenNode<std::string_view> const& keyword,
                                TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseClassItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseInstanceItem(TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseClassDefItem(TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}
ParseResult<Item> ParseSelectorsItem(TokenNode<std::string_view> const& keyword,
                                     TreeExprSpan& exprs) {
  return UnimplementedParseItem(keyword, exprs);
}

ParseResult<Item> ParseItem(absl::Span<list_tree::Expr const>& exprs) {
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenView(exprs));
  return GetItemParser(name.value())(name, exprs);
}
}  // namespace parsers::sci