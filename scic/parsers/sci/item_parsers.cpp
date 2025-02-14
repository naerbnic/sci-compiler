#include "scic/parsers/sci/item_parsers.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "scic/parsers/combinators/results.hpp"
#include "scic/parsers/combinators/status.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/parsers/sci/parser_common.hpp"
#include "util/choice.hpp"
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
        {"classdecl", ParseClassDeclItem},
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

ParseResult<ConstValue> ParseConstValue(list_tree::TokenExpr const& expr) {
  if (auto num = expr.token().AsNumber()) {
    return ConstValue(
        NumConstValue(TokenNode<int>(num->value, expr.text_range())));
  } else if (auto str = expr.token().AsString()) {
    return ConstValue(StringConstValue(
        TokenNode<std::string>(str->decodedString, expr.text_range())));
  } else {
    return RangeFailureOf(expr.text_range(), "Expected number or string.");
  }
}

ParseResult<ConstValue> ParseOneConstValue(TreeExprSpan& exprs) {
  return ParseOneTreeExpr(ParseTokenExpr(ParseConstValue))(exprs);
}

ParseResult<std::optional<InitialValue>> ParseInitialValue(
    TreeExprSpan& exprs) {
  if (StartsWith(IsIdentExprWith("="))(exprs)) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN(auto assign_token, ParseOneLiteralIdent("=")(exprs));
  return ParseOneTreeExpr(
      [](TreeExpr const& expr) -> ParseResult<InitialValue> {
        return expr.visit(
            [](list_tree::TokenExpr const& expr) -> ParseResult<InitialValue> {
              ASSIGN_OR_RETURN(auto value, ParseConstValue(expr));
              return InitialValue(std::move(value));
            },
            [](list_tree::ListExpr const& expr) -> ParseResult<InitialValue> {
              if (expr.kind() != list_tree::ListExpr::Kind::BRACKETS) {
                return RangeFailureOf(
                    expr.open_token().text_range(),
                    "Expected initial value to be in brackets.");
              }

              auto elements = expr.elements();

              ASSIGN_OR_RETURN(auto values,
                               ParseUntilComplete(ParseOneTreeExpr(
                                   ParseTokenExpr(ParseConstValue)))(elements));
              return InitialValue(ArrayInitialValue(std::move(values)));
            });
      })(exprs);
}

ParseResult<ModuleVarsDef> ParseModuleVarsDef(ModuleVarsDef::Kind kind,
                                              TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(
      auto entries,
      ParseUntilComplete(
          [](TreeExprSpan& exprs) -> ParseResult<ModuleVarsDef::Entry> {
            ASSIGN_OR_RETURN(auto name, ParseVarDef(exprs));
            ASSIGN_OR_RETURN(auto index, ParseOneNumberToken(exprs));
            ASSIGN_OR_RETURN(auto initial_value, ParseInitialValue(exprs));

            return ModuleVarsDef::Entry{
                .name = std::move(name),
                .index = std::move(index),
                .initial_value = std::move(initial_value),
            };
          })(exprs));

  return ModuleVarsDef(kind, std::move(entries));
}

ParseResult<ProcDef> ParseProcDef(TokenNode<std::string_view> const& keyword,
                                  TreeExprSpan& exprs) {
  // Format: ((method_name args... "&temp" locals...) body...)
  struct Signature {
    TokenNode<std::string> name;
    std::vector<TokenNode<std::string>> args;
    std::vector<VarDef> locals;
  };

  ASSIGN_OR_RETURN(
      auto signature,
      ParseOneListItem([](TreeExprSpan& exprs) -> ParseResult<Signature> {
        ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(exprs));
        // We need to find the "&temp" token, if it exists.
        auto temp_token_it = std::ranges::find_if(exprs.begin(), exprs.end(),
                                                  IsIdentExprWith("&temp"));
        TreeExprSpan params_span;
        TreeExprSpan locals_span;
        if (temp_token_it == exprs.end()) {
          params_span = exprs;
          locals_span = {};
        } else {
          std::size_t temp_index = temp_token_it - exprs.begin();
          params_span = exprs.subspan(0, temp_index);
          locals_span = exprs.subspan(temp_index + 1);
        }

        ASSIGN_OR_RETURN(
            auto args, ParseUntilComplete(ParseOneIdentTokenNode)(params_span));
        ASSIGN_OR_RETURN(auto locals,
                         ParseUntilComplete(ParseVarDef)(locals_span));
        return Signature{
            .name = std::move(name),
            .args = std::move(args),
            .locals = std::move(locals),
        };
      })(exprs));

  // FIXME: ParseExpr not yet implemented
  // ASSIGN_OR_RETURN(auto body, ParseUntilComplete(ParseExpr)(exprs));

  return ProcDef(std::move(signature.name), std::move(signature.args),
                 std::move(signature.locals), nullptr);
}

// Parser helper types for parsing a Class body.

struct ClassPropertiesBlock {
  std::vector<PropertyDef> properties;
};

class ClassDefInnerItem
    : public util::ChoiceBase<ClassDefInnerItem, ClassPropertiesBlock,
                              MethodNamesDecl, ProcDef> {
  using ChoiceBase::ChoiceBase;
};

ParseResult<PropertyDef> ParsePropertyDef(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto prop_name, ParseOneIdentTokenNode(exprs));
  ASSIGN_OR_RETURN(auto value, ParseOneConstValue(exprs));
  return PropertyDef{
      .name = std::move(prop_name),
      .value = std::move(value),
  };
}

ParseResult<ClassDefInnerItem> ParseClassDefInnerItem(TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenView(exprs));
  if (name.value() == "properties") {
    ASSIGN_OR_RETURN(auto properties,
                     ParseUntilComplete(ParsePropertyDef)(exprs));
    return ClassPropertiesBlock{
        .properties = std::move(properties),
    };
  } else if (name.value() == "methods") {
    ASSIGN_OR_RETURN(auto method_names,
                     ParseUntilComplete(ParseOneIdentTokenNode)(exprs));
    return MethodNamesDecl{
        .names = std::move(method_names),
    };
  } else if (name.value() == "method") {
    return ParseProcDef(name, exprs);
  } else {
    return RangeFailureOf(name.text_range(), "Unknown class item: %s",
                          name.value());
  }
}

ParseResult<ClassDef> ParseClassDef(ClassDef::Kind kind,
                                    TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs) {
  ASSIGN_OR_RETURN(auto name, ParseOneIdentTokenNode(exprs));

  std::optional<TokenNode<std::string>> parent;
  if (StartsWith(IsIdentExprWith("of"))(exprs)) {
    ASSIGN_OR_RETURN(auto of_token, ParseOneLiteralIdent("of")(exprs));
    ASSIGN_OR_RETURN(parent, ParseOneIdentTokenNode(exprs));
  }

  ASSIGN_OR_RETURN(auto inner_items,
                   ParseUntilComplete(ParseOneTreeExpr(
                       ParseListExpr(ParseClassDefInnerItem)))(exprs));

  std::optional<ClassPropertiesBlock> properties_block;
  std::optional<MethodNamesDecl> method_names_block;
  std::vector<ProcDef> methods;

  for (auto& inner_item : inner_items) {
    RETURN_IF_ERROR(
        std::move(inner_item)
            .visit(
                [&](ClassPropertiesBlock block) -> ParseStatus {
                  if (properties_block) {
                    return RangeFailureOf(
                        name.text_range(),
                        "Duplicate properties block in class definition.");
                  }
                  properties_block = std::move(block);
                  return ParseStatus::Ok();
                },
                [&](MethodNamesDecl block) {
                  if (method_names_block) {
                    return RangeFailureOf(
                        name.text_range(),
                        "Duplicate method names block in class definition.");
                  }
                  method_names_block = std::move(block);
                  return ParseStatus::Ok();
                },
                [&](ProcDef method) {
                  methods.push_back(std::move(method));
                  return ParseStatus::Ok();
                }));
  }

  if (!properties_block) {
    return RangeFailureOf(name.text_range(), "Missing properties block.");
  }

  return ClassDef(kind, std::move(name), std::move(parent),
                  std::move(properties_block->properties),
                  std::move(method_names_block), std::move(methods));
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
  return ParseModuleVarsDef(ModuleVarsDef::GLOBAL, exprs);
}

ParseResult<Item> ParseLocalItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs) {
  return ParseModuleVarsDef(ModuleVarsDef::LOCAL, exprs);
}

ParseResult<Item> ParseProcItem(TokenNode<std::string_view> const& keyword,
                                TreeExprSpan& exprs) {
  return ParseProcDef(keyword, exprs);
}

ParseResult<Item> ParseClassItem(TokenNode<std::string_view> const& keyword,
                                 TreeExprSpan& exprs) {
  return ParseClassDef(ClassDef::CLASS, keyword, exprs);
}

ParseResult<Item> ParseInstanceItem(TokenNode<std::string_view> const& keyword,
                                    TreeExprSpan& exprs) {
  return ParseClassDef(ClassDef::OBJECT, keyword, exprs);
}

ParseResult<Item> ParseClassDeclItem(TokenNode<std::string_view> const& keyword,
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