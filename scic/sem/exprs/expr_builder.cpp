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
#include "absl/types/span.h"
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

absl::Status BuildVarLoadExpr(ExprContext const* ctx,
                              FunctionBuilder::ValueOp val_op,
                              NameToken const& var_name,
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

        ctx->func_builder()->AddVarAccess(var_type, val_op, var_offset,
                                          /*add_accum_index=*/index != nullptr,
                                          std::string(var_name->view()));
        return absl::OkStatus();
      },
      [&](ExprContext::PropSym const& proc) -> absl::Status {
        if (index) {
          return absl::FailedPreconditionError("Properties cannot be indexed.");
        }
        ctx->func_builder()->AddPropAccess(val_op, proc.prop_offset,
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

absl::StatusOr<std::size_t> BuildCallArgs(ExprContext const* ctx,
                                          ast::CallArgs const& call_args) {
  std::size_t num_args = call_args.args().size();
  ctx->func_builder()->AddPushImmediate(num_args);
  for (auto const& arg : call_args.args()) {
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddPushOp();
  }
  if (call_args.rest()) {
    auto const& rest_name = call_args.rest()->rest_var;
    std::size_t param_offset = 1;
    if (rest_name) {
      auto const* rest_param = ctx->Lookup(rest_name->value());
      if (!rest_param) {
        return absl::NotFoundError(
            absl::StrFormat("Parameter '%s' not found.", rest_name->value()));
      }
      if (!rest_param->has<ExprContext::VarSym>() ||
          rest_param->as<ExprContext::VarSym>().has<ExprContext::ParamSym>()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Parameter '%s' is not a procedure/method parameter.",
            rest_name->value()));
      }
      param_offset = rest_param->as<ExprContext::VarSym>()
                         .as<ExprContext::ParamSym>()
                         .param_offset;
    }

    ctx->func_builder()->AddRestOp(param_offset);
  }

  return num_args;
}

template <FunctionBuilder::UnOp Op>
absl::Status BuildUnaryExpr(ExprContext const* ctx, NameToken const& op_name,
                            ast::CallArgs const& args) {
  if (args.args().size() != 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unary operator '%s' takes one argument.", op_name.value()));
  }
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unary operator '%s' cannot take a rest argument.", op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[0]));
  ctx->func_builder()->AddUnOp(Op);
  return absl::OkStatus();
}

template <FunctionBuilder::BinOp Op>
absl::Status BuildBinaryExpr(ExprContext const* ctx, NameToken const& op_name,
                             ast::CallArgs const& args) {
  if (args.args().size() != 2) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Binary operator '%s' takes two arguments.", op_name.value()));
  }
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Binary operator '%s' cannot take a rest argument.", op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[0]));
  ctx->func_builder()->AddPushOp();
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[1]));
  ctx->func_builder()->AddBinOp(Op);
  return absl::OkStatus();
}

template <FunctionBuilder::BinOp Op>
absl::Status BuildMultiExpr(ExprContext const* ctx, NameToken const& op_name,
                            ast::CallArgs const& args) {
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Multi-argument operator '%s' cannot take a rest argument.",
        op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Multi-argument operator '%s' must take at least one argument.",
        op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args[0]));
  for (auto const& arg : op_args.subspan(1)) {
    ctx->func_builder()->AddPushOp();
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBinOp(Op);
  }
  return absl::OkStatus();
}

// We need a specific function here, as the "-" operations is used for both
// negation and subtraction.
absl::Status BuildSubExpr(ExprContext const* ctx, NameToken const& op_name,
                          ast::CallArgs const& args) {
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Subtraction operator '%s' cannot take a rest argument.",
        op_name.value()));
  }
  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.size() == 1) {
    RETURN_IF_ERROR(ctx->BuildExpr(op_args[0]));
    ctx->func_builder()->AddUnOp(FunctionBuilder::NEG);
  } else if (op_args.size() == 2) {
    RETURN_IF_ERROR(ctx->BuildExpr(op_args[0]));
    ctx->func_builder()->AddPushOp();
    RETURN_IF_ERROR(ctx->BuildExpr(op_args[1]));
    ctx->func_builder()->AddBinOp(FunctionBuilder::SUB);
  } else {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Subtraction operator '%s' must take one or two arguments.",
        op_name.value()));
  }
  return absl::OkStatus();
}

// An implementation of the "and" operator. This short circuits, so it
// needs special handling.
absl::Status BuildAndExpr(ExprContext const* ctx, NameToken const& op_name,
                          ast::CallArgs const& args) {
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' cannot take a rest argument.", op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' must take at least one argument.", op_name.value()));
  }

  auto end_label = ctx->func_builder()->CreateLabelRef();
  for (auto const& arg : op_args.subspan(0, op_args.size() - 1)) {
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &end_label);
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args.back()));
  ctx->func_builder()->AddLabel(&end_label);
  return absl::OkStatus();
}
// An implementation of the "and" operator. This short circuits, so it
// needs special handling.
absl::Status BuildOrExpr(ExprContext const* ctx, NameToken const& op_name,
                         ast::CallArgs const& args) {
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' cannot take a rest argument.", op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' must take at least one argument.", op_name.value()));
  }

  auto end_label = ctx->func_builder()->CreateLabelRef();
  for (auto const& arg : op_args.subspan(0, op_args.size() - 1)) {
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, &end_label);
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args.back()));
  ctx->func_builder()->AddLabel(&end_label);
  return absl::OkStatus();
}

template <FunctionBuilder::ValueOp Op>
absl::Status BuildValueExpr(ExprContext const* ctx, NameToken const& op_name,
                            ast::CallArgs const& args) {
  if (args.rest()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Value operator '%s' cannot take a rest argument.", op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.size() != 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Operator '%s' must take exactly one argument.", op_name.value()));
  }

  auto const& arg = op_args[0];

  ASSIGN_OR_RETURN(auto var_name_and_index, GetVarNameAndIndex(ctx, arg));
  return BuildVarLoadExpr(ctx, Op, var_name_and_index.var_name,
                          var_name_and_index.index);
}

using CallFunc = absl::Status (*)(ExprContext const* ctx,
                                  NameToken const& op_name,
                                  ast::CallArgs const& call);

std::map<std::string_view, CallFunc> const& GetCallBuiltins() {
  static std::map<std::string_view, CallFunc> builtins = {
      {"-", &BuildSubExpr},
      {"not", &BuildUnaryExpr<FunctionBuilder::NOT>},
      {"~", &BuildUnaryExpr<FunctionBuilder::BNOT>},
      {"/", &BuildBinaryExpr<FunctionBuilder::DIV>},
      {"<<", &BuildBinaryExpr<FunctionBuilder::SHL>},
      {">>", &BuildBinaryExpr<FunctionBuilder::SHR>},
      {"%", &BuildBinaryExpr<FunctionBuilder::MOD>},
      {"<", &BuildBinaryExpr<FunctionBuilder::LT>},
      {"<=", &BuildBinaryExpr<FunctionBuilder::LE>},
      {">", &BuildBinaryExpr<FunctionBuilder::GT>},
      {">=", &BuildBinaryExpr<FunctionBuilder::GE>},
      {"==", &BuildBinaryExpr<FunctionBuilder::EQ>},
      {"!=", &BuildBinaryExpr<FunctionBuilder::NE>},
      {"u<", &BuildBinaryExpr<FunctionBuilder::ULT>},
      {"u<=", &BuildBinaryExpr<FunctionBuilder::ULE>},
      {"u>", &BuildBinaryExpr<FunctionBuilder::UGT>},
      {"u>=", &BuildBinaryExpr<FunctionBuilder::UGE>},
      {"+", &BuildMultiExpr<FunctionBuilder::ADD>},
      {"*", &BuildMultiExpr<FunctionBuilder::MUL>},
      {"|", &BuildMultiExpr<FunctionBuilder::OR>},
      {"&", &BuildMultiExpr<FunctionBuilder::AND>},
      {"^", &BuildMultiExpr<FunctionBuilder::XOR>},
      {"and", &BuildAndExpr},
      {"or", &BuildOrExpr},
      {"++", &BuildValueExpr<FunctionBuilder::INC>},
      {"--", &BuildValueExpr<FunctionBuilder::DEC>},
  };
  return builtins;
}

absl::Status BuildCallExpr(ExprContext const* ctx, ast::CallExpr const& call) {
  ASSIGN_OR_RETURN(auto num_args, BuildCallArgs(ctx, call.call_args()));
  auto const& target = call.target();
  // The original appears to only support calls to names, but I think
  // there's a reason that I made this support general expressions.
  if (!target.has<ast::VarExpr>()) {
    return absl::InvalidArgumentError(
        "Only calls to names are supported at this time.");
  }
  auto const& target_name = target.as<ast::VarExpr>().name();

  // A number of operations are represented as calls to built-in functions.
  auto const& builtins = GetCallBuiltins();
  auto it = builtins.find(target_name.value());
  if (it != builtins.end()) {
    return it->second(ctx, target_name, call.call_args());
  }

  auto const* proc = ctx->LookupProc(target_name.value());
  return proc->visit(
      [&](ExprContext::LocalProc const& local) -> absl::Status {
        ctx->func_builder()->AddProcCall(std::string(local.name.value()),
                                         num_args, local.proc_ref);
        return absl::OkStatus();
      },
      [&](ExprContext::ExternProc const& ext) -> absl::Status {
        ctx->func_builder()->AddExternCall(std::string(ext.name.value()),
                                           num_args, ext.script_num.value(),
                                           ext.extern_offset);
        return absl::OkStatus();
      },
      [&](ExprContext::KernelProc const& kernel) -> absl::Status {
        ctx->func_builder()->AddKernelCall(std::string(kernel.name.value()),
                                           num_args, kernel.kernel_offset);
        return absl::OkStatus();
      });
}

absl::StatusOr<std::size_t> BuildSendClause(ExprContext const* ctx,
                                            ast::SendClause const& clause) {
  struct SelectorNameAndArgs {
    NameToken const& sel_name;
    ast::CallArgs const* args;
  };
  auto sel_and_args = clause.visit(
      [&](ast::PropReadSendClause const& prop_read) {
        return SelectorNameAndArgs{prop_read.prop_name(), nullptr};
      },
      [&](ast::MethodSendClause const& method_send) {
        return SelectorNameAndArgs{method_send.selector(),
                                   &method_send.call_args()};
      });

  // I just learned that the selector name is looked up in the symbol context
  // before being looked up in selector context, allowing code like:
  //
  // (procedure (Eval obj sel)
  //   (obj sel: &rest)
  // )
  //
  // We see if we have a non-prop variable, and look it up to support this.

  auto const* symbol = ctx->Lookup(sel_and_args.sel_name.value());

  if (symbol && symbol->has<ExprContext::VarSym>()) {
    // Not maximally efficient, but it should work.
    RETURN_IF_ERROR(BuildVarLoadExpr(ctx, FunctionBuilder::LOAD,
                                     sel_and_args.sel_name, nullptr));
    ctx->func_builder()->AddPushOp();
  } else {
    auto const* selector =
        ctx->selector_table()->LookupByName(sel_and_args.sel_name.value());
    if (!selector) {
      return absl::NotFoundError(absl::StrFormat(
          "Selector '%s' not found.", sel_and_args.sel_name.value()));
    }
    ctx->func_builder()->AddPushImmediate(
        int(selector->selector_num().value()));
  }

  if (sel_and_args.args) {
    ASSIGN_OR_RETURN(auto num_args, BuildCallArgs(ctx, *sel_and_args.args));
    return num_args + 1;
  } else {
    ctx->func_builder()->AddPushImmediate(0);
    return 2;
  }
}

absl::Status BuildSendExpr(ExprContext const* ctx, ast::SendExpr const& send) {
  std::size_t num_args = 0;
  for (auto const& clause : send.clauses()) {
    ASSIGN_OR_RETURN(auto clause_num_args, BuildSendClause(ctx, clause));
    num_args += clause_num_args;
  }

  return send.target().visit(
      [&](ast::SelfSendTarget const& self_target) {
        ctx->func_builder()->AddSelfSend(num_args);
        return absl::OkStatus();
      },
      [&](ast::SuperSendTarget const& index_expr) -> absl::Status {
        if (!ctx->super_info()) {
          return absl::FailedPreconditionError(
              "Cannot send to super without a super class.");
        }
        auto const& super_info = *ctx->super_info();
        ctx->func_builder()->AddSuperSend(
            std::string(super_info.super_name.value()), num_args,
            int(super_info.species.value()));
        return absl::OkStatus();
      },
      [&](ast::ExprSendTarget const& expr) -> absl::Status {
        RETURN_IF_ERROR(ctx->BuildExpr(expr.target()));
        ctx->func_builder()->AddSend(num_args);
        return absl::OkStatus();
      });
}

absl::Status BuildAssignExpr(ExprContext const* ctx,
                             ast::AssignExpr const& assign) {
  auto assign_op = BuildAssignOp(assign.kind());
  ASSIGN_OR_RETURN(auto result, GetVarNameAndIndex(ctx, assign.target()));
  if (assign_op) {
    RETURN_IF_ERROR(BuildVarLoadExpr(ctx, FunctionBuilder::LOAD,
                                     result.var_name, result.index));
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

absl::Status BuildBreakExpr(ExprContext const* ctx,
                            ast::BreakExpr const& break_expr) {
  std::size_t level = 0;
  if (break_expr.level()) {
    level = break_expr.level()->value();
  }
  auto* break_label = ctx->GetBreakLabel(level);
  if (!break_label) {
    return absl::FailedPreconditionError(
        "Cannot break to level without a loop.");
  }
  if (break_expr.condition()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**break_expr.condition()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, break_label);
  } else {
    ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, break_label);
  }
  return absl::OkStatus();
}

absl::Status BuildContEpxr(ExprContext const* ctx,
                           ast::ContinueExpr const& break_expr) {
  std::size_t level = 0;
  if (break_expr.level()) {
    level = break_expr.level()->value();
  }
  auto* break_label = ctx->GetContLabel(level);
  if (!break_label) {
    return absl::FailedPreconditionError(
        "Cannot break to level without a loop.");
  }
  if (break_expr.condition()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**break_expr.condition()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, break_label);
  } else {
    ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, break_label);
  }
  return absl::OkStatus();
}

absl::Status BuildIfExpr(ExprContext const* ctx, ast::IfExpr const& if_expr) {
  RETURN_IF_ERROR(ctx->BuildExpr(if_expr.condition()));
  if (if_expr.else_body()) {
    auto end_label = ctx->func_builder()->CreateLabelRef();
    auto else_label = ctx->func_builder()->CreateLabelRef();
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &else_label);
    RETURN_IF_ERROR(ctx->BuildExpr(if_expr.then_body()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &end_label);
    ctx->func_builder()->AddLabel(&else_label);
    RETURN_IF_ERROR(ctx->BuildExpr(**if_expr.else_body()));
    ctx->func_builder()->AddLabel(&end_label);
  } else {
    auto end_label = ctx->func_builder()->CreateLabelRef();
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &end_label);
    RETURN_IF_ERROR(ctx->BuildExpr(if_expr.then_body()));
    ctx->func_builder()->AddLabel(&end_label);
  }
  return absl::OkStatus();
}

absl::Status BuildCondExpr(ExprContext const* ctx,
                           ast::CondExpr const& cond_expr) {
  auto done = ctx->func_builder()->CreateLabelRef();
  for (std::size_t i = 0; i < cond_expr.branches().size(); ++i) {
    auto next = ctx->func_builder()->CreateLabelRef();
    auto const& branch = cond_expr.branches()[i];
    bool at_end =
        (i == cond_expr.branches().size() - 1) && !cond_expr.else_body();
    RETURN_IF_ERROR(ctx->BuildExpr(*branch.condition));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT,
                                     at_end ? &done : &next);
    RETURN_IF_ERROR(ctx->BuildExpr(*branch.body));
    if (!at_end) {
      ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &done);
      ctx->func_builder()->AddLabel(&next);
    }
  }

  if (cond_expr.else_body()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**cond_expr.else_body()));
  }
  ctx->func_builder()->AddLabel(&done);
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
          return BuildVarLoadExpr(this, FunctionBuilder::LOAD, var_ref.name(),
                                  nullptr);
        },
        [&](ast::ArrayIndexExpr const& array_index) {
          return BuildVarLoadExpr(this, FunctionBuilder::LOAD,
                                  array_index.var_name(), &array_index.index());
        },
        [&](ast::CallExpr const& selector_ref) {
          return BuildCallExpr(this, selector_ref);
        },
        [&](ast::ReturnExpr const& array_ref) {
          if (array_ref.expr()) {
            RETURN_IF_ERROR(BuildExpr(**array_ref.expr()));
          }
          func_builder()->AddReturnOp();
          return absl::OkStatus();
        },
        [&](ast::BreakExpr const& break_expr) {
          return BuildBreakExpr(this, break_expr);
        },
        [&](ast::ContinueExpr const& cont_expr) {
          return BuildContEpxr(this, cont_expr);
        },
        [&](ast::WhileExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::ForExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::IfExpr const& if_expr) { return BuildIfExpr(this, if_expr); },
        [&](ast::CondExpr const& cond_expr) {
          return BuildCondExpr(this, cond_expr);
        },
        [&](ast::SwitchExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::SwitchToExpr const& object_ref) {
          return absl::UnimplementedError("unimplemented");
        },
        [&](ast::SendExpr const& object_ref) {
          return BuildSendExpr(this, object_ref);
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
    std::optional<ExprContext::SuperInfo> super_info,
    std::map<std::string_view, ExprContext::Sym, std::less<>> symbols,
    std::map<std::string_view, ExprContext::Proc, std::less<>> procs) {
  return std::make_unique<ExprContextImpl>(
      codegen, func_builder, class_table, selector_table, std::move(super_info),
      std::move(symbols), std::move(procs));
}

}  // namespace sem