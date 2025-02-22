#include "scic/sem/exprs/expr_builder.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/exprs/expr_context.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/status/status_macros.hpp"

namespace sem {

namespace {

using codegen::FunctionBuilder;

using VarType = FunctionBuilder::VarType;

struct VarAndIndex {
  NameToken const& var_name;
  ast::Expr const* index;
};

absl::StatusOr<VarAndIndex> GetVarNameAndIndex(ExprContext const* ctx,
                                               ast::Expr const& expr) {
  return expr.visit(
      [&](ast::VarExpr const& var_ref) -> absl::StatusOr<VarAndIndex> {
        return VarAndIndex{var_ref.name(), nullptr};
      },
      [&](ast::ArrayIndexExpr const& index_expr)
          -> absl::StatusOr<VarAndIndex> {
        return VarAndIndex{index_expr.var_name(), &index_expr.index()};
      },
      [&](auto&&) -> absl::StatusOr<VarAndIndex> {
        return absl::UnimplementedError(
            "An expression must either be a variable or an array index expr.");
      });
}

struct VarTypeAndOffset {
  VarType type;
  std::size_t offset;
};

VarTypeAndOffset GetVarTypeAndOffset(ExprContext::VarSym const& var_sym) {
  return var_sym.visit(
      [&](ExprContext::GlobalSym const& global) -> VarTypeAndOffset {
        return {VarType::GLOBAL, global.global_offset};
      },
      [&](ExprContext::TempSym const& temp) -> VarTypeAndOffset {
        return {VarType::TEMP, temp.temp_offset};
      },
      [&](ExprContext::ParamSym const& param) -> VarTypeAndOffset {
        return {VarType::PARAM, param.param_offset};
      },
      [&](ExprContext::LocalSym const& local) -> VarTypeAndOffset {
        return {VarType::LOCAL, local.local_offset};
      });
}

absl::Status BuildAddrOfExpr(ExprContext const* ctx,
                             ast::AddrOfExpr const& addr_of) {
  ASSIGN_OR_RETURN(auto const& result, GetVarNameAndIndex(ctx, addr_of.expr()));
  auto const& [var_name, index] = result;

  auto const* sym = ctx->Lookup(var_name.value());
  if (!sym) {
    // We really should be using a type that can take text ranges.
    return absl::NotFoundError(
        absl::StrFormat("Variable '%s' not found.", var_name.value()));
  }

  ASSIGN_OR_RETURN(
      auto type_val,
      sym->visit(
          [&](ExprContext::VarSym const& var_sym)
              -> absl::StatusOr<VarTypeAndOffset> {
            return GetVarTypeAndOffset(var_sym);
          },
          [&](ExprContext::PropSym const& proc)
              -> absl::StatusOr<VarTypeAndOffset> {
            return absl::FailedPreconditionError(
                "Properties cannot be used in an AddrOf expression.");
          }));

  if (index) {
    RETURN_IF_ERROR(ctx->BuildExpr(*index));
  }
  ctx->func_builder()->AddLoadVarAddr(type_val.type, type_val.offset,
                                      /*add_accum_index=*/index != nullptr,
                                      std::string(var_name->view()));
  return absl::OkStatus();
}

absl::Status BuildSelectLitExpr(ExprContext const* ctx,
                                ast::SelectLitExpr const& select_lit) {
  auto const* selector =
      ctx->selector_table()->LookupByName(select_lit.selector().value());
  if (!selector) {
    return absl::NotFoundError(absl::StrFormat("Selector '%s' not found.",
                                               select_lit.selector().value()));
  }
  ctx->func_builder()->AddLoadImmediate(int(selector->selector_num().value()));
  return absl::OkStatus();
}

absl::Status BuildConstExpr(ExprContext const* ctx,
                            ast::ConstValue const& const_value) {
  ASSIGN_OR_RETURN(
      codegen::LiteralValue value,
      const_value.visit(
          [&](ast::NumConstValue const& num)
              -> absl::StatusOr<codegen::LiteralValue> {
            ASSIGN_OR_RETURN(auto machine_value,
                             ConvertToMachineWord(num.value().value()));
            return codegen::LiteralValue(machine_value);
          },
          [&](ast::StringConstValue const& str)
              -> absl::StatusOr<codegen::LiteralValue> {
            ;
            return ctx->codegen()->AddTextNode(str.value().value());
          }));

  ctx->func_builder()->AddLoadImmediate(value);
  return absl::OkStatus();
}

absl::Status BuildVarStoreExpr(ExprContext const* ctx,
                               NameToken const& var_name,
                               ast::Expr const* index) {
  // We're going to store the accumulator. Push it onto the stack.
  ctx->func_builder()->AddPushOp();

  auto const* sym = ctx->Lookup(var_name.value());
  if (!sym) {
    // We really should be using a type that can take text ranges.
    return absl::NotFoundError(
        absl::StrFormat("Variable '%s' not found.", var_name.value()));
  }
  return sym->visit(
      [&](ExprContext::VarSym const& var_sym) -> absl::Status {
        auto const& [var_type, var_offset] = GetVarTypeAndOffset(var_sym);
        if (index) {
          RETURN_IF_ERROR(ctx->BuildExpr(*index));
        }

        ctx->func_builder()->AddVarAccess(var_type, FunctionBuilder::STORE,
                                          var_offset,
                                          /*add_accum_index=*/index != nullptr,
                                          std::string(var_name->view()));
        return absl::OkStatus();
      },
      [&](ExprContext::PropSym const& proc) -> absl::Status {
        if (index) {
          return absl::FailedPreconditionError("Properties cannot be indexed.");
        }
        ctx->func_builder()->AddPropAccess(FunctionBuilder::STORE,
                                           proc.prop_offset,
                                           std::string(proc.selector->name()));
        return absl::OkStatus();
      });
}

absl::Status BuildVarLoadExpr(ExprContext const* ctx, NameToken const& var_name,
                              absl::Nullable<ast::Expr const*> index) {
  auto const* sym = ctx->Lookup(var_name.value());
  if (!sym) {
    // We really should be using a type that can take text ranges.
    return absl::NotFoundError(
        absl::StrFormat("Variable '%s' not found.", var_name.value()));
  }

  return sym->visit(
      [&](ExprContext::VarSym const& var_sym) -> absl::Status {
        auto const& [var_type, var_offset] = GetVarTypeAndOffset(var_sym);
        if (index) {
          RETURN_IF_ERROR(ctx->BuildExpr(*index));
        }

        ctx->func_builder()->AddVarAccess(var_type, FunctionBuilder::LOAD,
                                          var_offset,
                                          /*add_accum_index=*/index != nullptr,
                                          std::string(var_name->view()));
        return absl::OkStatus();
      },
      [&](ExprContext::PropSym const& proc) -> absl::Status {
        if (index) {
          return absl::FailedPreconditionError("Properties cannot be indexed.");
        }
        ctx->func_builder()->AddPropAccess(FunctionBuilder::LOAD,
                                           proc.prop_offset,
                                           std::string(proc.selector->name()));
        return absl::OkStatus();
      });
}

std::optional<FunctionBuilder::BinOp> BuildAssignOp(
    parsers::sci::AssignExpr::Kind kind) {
  switch (kind) {
    case parsers::sci::AssignExpr::DIRECT:
      return std::nullopt;
    case parsers::sci::AssignExpr::ADD:
      return FunctionBuilder::ADD;
    case parsers::sci::AssignExpr::SUB:
      return FunctionBuilder::SUB;
    case parsers::sci::AssignExpr::MUL:
      return FunctionBuilder::MUL;
    case parsers::sci::AssignExpr::DIV:
      return FunctionBuilder::DIV;
    case parsers::sci::AssignExpr::MOD:
      return FunctionBuilder::MOD;
    case parsers::sci::AssignExpr::AND:
      return FunctionBuilder::AND;
    case parsers::sci::AssignExpr::OR:
      return FunctionBuilder::OR;
    case parsers::sci::AssignExpr::XOR:
      return FunctionBuilder::XOR;
    case parsers::sci::AssignExpr::SHR:
      return FunctionBuilder::SHR;
    case parsers::sci::AssignExpr::SHL:
      return FunctionBuilder::SHL;
  }
}

absl::Status BuildAssignExpr(ExprContext const* ctx,
                             ast::AssignExpr const& assign) {
  auto assign_op = BuildAssignOp(assign.kind());
  ASSIGN_OR_RETURN(auto result, GetVarNameAndIndex(ctx, assign.target()));
  if (assign_op) {
    RETURN_IF_ERROR(BuildVarLoadExpr(ctx, result.var_name, result.index));
    ctx->func_builder()->AddPushOp();
    RETURN_IF_ERROR(ctx->BuildExpr(assign.value()));
    ctx->func_builder()->AddBinOp(*assign_op);
  } else {
    RETURN_IF_ERROR(ctx->BuildExpr(assign.value()));
  }

  RETURN_IF_ERROR(BuildVarStoreExpr(ctx, result.var_name, result.index));
  return absl::OkStatus();
}

absl::Status BuildExprList(ExprContext const* ctx,
                           ast::ExprList const& expr_list) {
  for (auto const& expr : expr_list.exprs()) {
    RETURN_IF_ERROR(ctx->BuildExpr(expr));
  }
  return absl::OkStatus();
}

class ExprContextImpl : public ExprContext {
 public:
  using ExprContext::ExprContext;

  absl::Status BuildExpr(ast::Expr const& expr) const override {
    return expr.visit(
        [&](ast::AddrOfExpr const& binary) {
          return BuildAddrOfExpr(this, binary);
        },
        [&](ast::SelectLitExpr const& select_lit) {
          return BuildSelectLitExpr(this, select_lit);
        },
        [&](ast::ConstValueExpr const& const_expr) {
          return BuildConstExpr(this, const_expr.value());
        },
        [&](ast::VarExpr const& var_ref) {
          return BuildVarLoadExpr(this, var_ref.name(), nullptr);
        },
        [&](ast::ArrayIndexExpr const& array_index) {
          return BuildVarLoadExpr(this, array_index.var_name(),
                                  &array_index.index());
        },
        [&](ast::CallExpr const& selector_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::ReturnExpr const& array_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::BreakExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::ContinueExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::WhileExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::ForExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::IfExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::CondExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::SwitchExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::SwitchToExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::SendExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::AssignExpr const& object_ref) {
          return BuildAssignExpr(this, object_ref);
        },
        [&](ast::ExprList const& object_ref) {
          return BuildExprList(this, object_ref);
        });
    return absl::OkStatus();
  }
};

}  // namespace

std::unique_ptr<ExprContext> CreateExprContext(
    codegen::CodeGenerator* codegen, codegen::FunctionBuilder* func_builder,
    ClassTable const* class_table, SelectorTable const* selector_table,
    std::map<std::string_view, ExprContext::Sym, std::less<>> symbols) {
  return std::make_unique<ExprContextImpl>(codegen, func_builder, class_table,
                                           selector_table, std::move(symbols));
}

}  // namespace sem