#include "scic/parsers/sci/item_parsers.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::sci {

namespace {

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

ParseResult<VarDef> ParseVarDef(TreeExprSpan& exprs) {
  return ParseOneTreeExpr([](TreeExpr const& expr) -> ParseResult<VarDef> {
    return expr.visit(
        [](list_tree::ListExpr const& list_expr) -> ParseResult<VarDef> {
          if (list_expr.kind() != list_tree::ListExpr::Kind::BRACKETS) {
            return RangeFailureOf(
                list_expr.open_token().text_range(),
                "Expected variable definition to be in parentheses.");
          }
          auto elements = list_expr.elements();
          return ParseComplete([](TreeExprSpan& exprs) -> ParseResult<VarDef> {
            ASSIGN_OR_RETURN(auto var_name, ParseOneIdentTokenNode(exprs));
            ASSIGN_OR_RETURN(auto size, ParseOneNumberToken(exprs));
            return VarDef(ArrayVarDef(std::move(var_name), std::move(size)));
          })(elements);
        },
        [](list_tree::TokenExpr const& expr) -> ParseResult<VarDef> {
          ASSIGN_OR_RETURN(
              auto var_name,
              ParseTokenExpr(ParseIdentToken(ParseSimpleIdentNameNode))(expr));
          return VarDef(SingleVarDef(std::move(var_name)));
        });
  })(exprs);
}

}  // namespace

ParseResult<Item> ParsePublicItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(
      auto entries,
      ParseUntilComplete(
          [](TreeExprSpan& exprs) -> ParseResult<PublicDef::Entry> {
            ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(exprs));
            ASSIGN_OR_RETURN(auto index, ParseOneNumberToken(exprs));

            return PublicDef::Entry{
                .name = name,
                .index = index,
            };
          })(exprs));

  return Item(PublicDef(std::move(entries)));
}

ParseResult<Item> ParseExternItem(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(
      auto entries,
      ParseUntilComplete(
          [](TreeExprSpan& exprs) -> ParseResult<ExternDef::Entry> {
            ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(exprs));
            ASSIGN_OR_RETURN(auto module_num, ParseOneNumberToken(exprs));
            ASSIGN_OR_RETURN(auto index, ParseOneNumberToken(exprs));

            return ExternDef::Entry{
                .name = name,
                .module_num = module_num,
                .index = index,
            };
          })(exprs));

  return Item(ExternDef(std::move(entries)));
}

ParseResult<Item> ParseGlobalDeclItem(
    TokenNode<std::string_view> const& keyword, TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(
      auto entries,
      ParseUntilComplete(
          [](TreeExprSpan& exprs) -> ParseResult<GlobalDeclDef::Entry> {
            ASSIGN_OR_RETURN(auto name, ParseVarDef(exprs));
            ASSIGN_OR_RETURN(auto index, ParseOneNumberToken(exprs));

            return GlobalDeclDef::Entry{
                .name = std::move(name),
                .index = index,
            };
          })(exprs));

  return Item(GlobalDeclDef(std::move(entries)));
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