#include "scic/sem/exprs/expr_builder.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/exprs/expr_context.hpp"
#include "scic/sem/selector_table.hpp"
#include "scic/status/status.hpp"
#include "util/status/status_macros.hpp"

namespace sem {

namespace {

using codegen::FunctionBuilder;

using VarType = FunctionBuilder::VarType;

struct VarAndIndex {
  NameToken const& var_name;
  ast::Expr const* index;
};

// Checks if the given expression is a variable expression or an array index
// expression. Returns an error otherwise.
VarAndIndex GetVarNameAndIndex(ast::LValueExpr const& expr) {
  return expr.visit(
      [&](ast::VarExpr const& var_ref) -> VarAndIndex {
        return VarAndIndex{var_ref.name(), nullptr};
      },
      [&](ast::ArrayIndexExpr const& index_expr) -> VarAndIndex {
        return VarAndIndex{index_expr.var_name(), &index_expr.index()};
      });
}

struct VarTypeAndOffset {
  VarType type;
  std::size_t offset;
};

VarTypeAndOffset GetVarTypeAndOffset(ExprEnvironment::VarSym const& var_sym) {
  return var_sym.visit(
      [&](ExprEnvironment::GlobalSym const& global) -> VarTypeAndOffset {
        return {VarType::GLOBAL, global.global_offset};
      },
      [&](ExprEnvironment::TempSym const& temp) -> VarTypeAndOffset {
        return {VarType::TEMP, temp.temp_offset};
      },
      [&](ExprEnvironment::ParamSym const& param) -> VarTypeAndOffset {
        return {VarType::PARAM, param.param_offset};
      },
      [&](ExprEnvironment::LocalSym const& local) -> VarTypeAndOffset {
        return {VarType::LOCAL, local.local_offset};
      });
}

status::Status BuildAddrOfExpr(ExprContext* ctx,
                               ast::AddrOfExpr const& addr_of) {
  auto const& [var_name, index] = GetVarNameAndIndex(addr_of.expr());

  ASSIGN_OR_RETURN(auto sym, ctx->LookupSym(var_name.value()));

  ASSIGN_OR_RETURN(auto type_val,
                   sym.visit(
                       [&](ExprEnvironment::VarSym const& var_sym)
                           -> status::StatusOr<VarTypeAndOffset> {
                         return GetVarTypeAndOffset(var_sym);
                       },
                       [&](ExprEnvironment::PropSym const& proc)
                           -> status::StatusOr<VarTypeAndOffset> {
                         return status::FailedPreconditionError(
                             "Properties cannot be used in an "
                             "AddrOf expression.");
                       }));

  if (index) {
    RETURN_IF_ERROR(ctx->BuildExpr(*index));
  }
  ctx->func_builder()->AddLoadVarAddr(type_val.type, type_val.offset,
                                      /*add_accum_index=*/index != nullptr,
                                      std::string(var_name->view()));
  return status::OkStatus();
}

status::Status BuildSelectLitExpr(ExprContext* ctx,
                                  ast::SelectLitExpr const& select_lit) {
  auto selector = ctx->LookupSelector(select_lit.selector().value());
  if (!selector) {
    return status::NotFoundError(absl::StrFormat(
        "Selector '%s' not found.", select_lit.selector().value()));
  }
  ctx->func_builder()->AddLoadImmediate(int(selector->value()));
  return status::OkStatus();
}

status::Status BuildConstExpr(ExprContext* ctx,
                              ast::ConstValue const& const_value) {
  ASSIGN_OR_RETURN(
      codegen::LiteralValue value,
      const_value.visit(
          [&](ast::NumConstValue const& num)
              -> status::StatusOr<codegen::LiteralValue> {
            ASSIGN_OR_RETURN(auto machine_value,
                             ConvertToMachineWord(num.value().value()));
            return codegen::LiteralValue(machine_value);
          },
          [&](ast::StringConstValue const& str)
              -> status::StatusOr<codegen::LiteralValue> {
            ;
            return ctx->codegen()->AddTextNode(str.value().value());
          }));

  ctx->func_builder()->AddLoadImmediate(value);
  return status::OkStatus();
}

status::Status BuildVarStoreExpr(ExprContext* ctx, NameToken const& var_name,
                                 ast::Expr const* index) {
  // We're going to store the accumulator. Push it onto the stack.
  ctx->func_builder()->AddPushOp();

  ASSIGN_OR_RETURN(auto sym, ctx->LookupSym(var_name.value()));
  return sym.visit(
      [&](ExprEnvironment::VarSym const& var_sym) -> status::Status {
        auto const& [var_type, var_offset] = GetVarTypeAndOffset(var_sym);
        if (index) {
          RETURN_IF_ERROR(ctx->BuildExpr(*index));
        }

        ctx->func_builder()->AddVarAccess(var_type, FunctionBuilder::STORE,
                                          var_offset,
                                          /*add_accum_index=*/index != nullptr,
                                          std::string(var_name->view()));
        return status::OkStatus();
      },
      [&](ExprEnvironment::PropSym const& proc) -> status::Status {
        if (index) {
          return status::FailedPreconditionError(
              "Properties cannot be indexed.");
        }
        ctx->func_builder()->AddPropAccess(FunctionBuilder::STORE,
                                           proc.prop_offset,
                                           std::string(proc.selector->name()));
        return status::OkStatus();
      });
}

status::Status BuildVarLoadExpr(ExprContext* ctx,
                                FunctionBuilder::ValueOp val_op,
                                NameToken const& var_name,
                                absl::Nullable<ast::Expr const*> index) {
  ASSIGN_OR_RETURN(auto sym, ctx->LookupSym(var_name.value()));
  return sym.visit(
      [&](ExprEnvironment::VarSym const& var_sym) -> status::Status {
        auto const& [var_type, var_offset] = GetVarTypeAndOffset(var_sym);
        if (index) {
          RETURN_IF_ERROR(ctx->BuildExpr(*index));
        }

        ctx->func_builder()->AddVarAccess(var_type, val_op, var_offset,
                                          /*add_accum_index=*/index != nullptr,
                                          std::string(var_name->view()));
        return status::OkStatus();
      },
      [&](ExprEnvironment::PropSym const& proc) -> status::Status {
        if (index) {
          return status::FailedPreconditionError(
              "Properties cannot be indexed.");
        }
        ctx->func_builder()->AddPropAccess(val_op, proc.prop_offset,
                                           std::string(proc.selector->name()));
        return status::OkStatus();
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

status::StatusOr<std::size_t> BuildCallArgs(ExprContext* ctx,
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
      ASSIGN_OR_RETURN(auto rest_param, ctx->LookupSym(rest_name->value()));
      if (!rest_param.has<ExprEnvironment::VarSym>() ||
          rest_param.as<ExprEnvironment::VarSym>()
              .has<ExprEnvironment::ParamSym>()) {
        return status::FailedPreconditionError(absl::StrFormat(
            "Parameter '%s' is not a procedure/method parameter.",
            rest_name->value()));
      }
      param_offset = rest_param.as<ExprEnvironment::VarSym>()
                         .as<ExprEnvironment::ParamSym>()
                         .param_offset;
    }

    ctx->func_builder()->AddRestOp(param_offset);
  }

  return num_args;
}

template <FunctionBuilder::UnOp Op>
status::Status BuildUnaryExpr(ExprContext* ctx, NameToken const& op_name,
                              ast::CallArgs const& args) {
  if (args.args().size() != 1) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Unary operator '%s' takes one argument.", op_name.value()));
  }
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Unary operator '%s' cannot take a rest argument.", op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[0]));
  ctx->func_builder()->AddUnOp(Op);
  return status::OkStatus();
}

template <FunctionBuilder::BinOp Op>
status::Status BuildBinaryExpr(ExprContext* ctx, NameToken const& op_name,
                               ast::CallArgs const& args) {
  if (args.args().size() != 2) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Binary operator '%s' takes two arguments.", op_name.value()));
  }
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Binary operator '%s' cannot take a rest argument.", op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[0]));
  ctx->func_builder()->AddPushOp();
  RETURN_IF_ERROR(ctx->BuildExpr(args.args()[1]));
  ctx->func_builder()->AddBinOp(Op);
  return status::OkStatus();
}

template <FunctionBuilder::BinOp Op>
status::Status BuildMultiExpr(ExprContext* ctx, NameToken const& op_name,
                              ast::CallArgs const& args) {
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Multi-argument operator '%s' cannot take a rest argument.",
        op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Multi-argument operator '%s' must take at least one argument.",
        op_name.value()));
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args[0]));
  for (auto const& arg : op_args.subspan(1)) {
    ctx->func_builder()->AddPushOp();
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBinOp(Op);
  }
  return status::OkStatus();
}

// We need a specific function here, as the "-" operations is used for both
// negation and subtraction.
status::Status BuildSubExpr(ExprContext* ctx, NameToken const& op_name,
                            ast::CallArgs const& args) {
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
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
    return status::InvalidArgumentError(absl::StrFormat(
        "Subtraction operator '%s' must take one or two arguments.",
        op_name.value()));
  }
  return status::OkStatus();
}

// An implementation of the "and" operator. This short circuits, so it
// needs special handling.
status::Status BuildAndExpr(ExprContext* ctx, NameToken const& op_name,
                            ast::CallArgs const& args) {
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' cannot take a rest argument.", op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' must take at least one argument.", op_name.value()));
  }

  auto end_label = ctx->func_builder()->CreateLabelRef();
  for (auto const& arg : op_args.subspan(0, op_args.size() - 1)) {
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &end_label);
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args.back()));
  ctx->func_builder()->AddLabel(&end_label);
  return status::OkStatus();
}
// An implementation of the "and" operator. This short circuits, so it
// needs special handling.
status::Status BuildOrExpr(ExprContext* ctx, NameToken const& op_name,
                           ast::CallArgs const& args) {
  if (args.rest()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' cannot take a rest argument.", op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.empty()) {
    return status::InvalidArgumentError(absl::StrFormat(
        "And operator '%s' must take at least one argument.", op_name.value()));
  }

  auto end_label = ctx->func_builder()->CreateLabelRef();
  for (auto const& arg : op_args.subspan(0, op_args.size() - 1)) {
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, &end_label);
  }
  RETURN_IF_ERROR(ctx->BuildExpr(op_args.back()));
  ctx->func_builder()->AddLabel(&end_label);
  return status::OkStatus();
}

// Builds a comparison expression. This works as an n-ary operator, which
// pairwise compares all of the arguments. It short-circuits to false
// when the first comparison fails.
template <FunctionBuilder::BinOp Op>
status::Status BuildCompExpr(ExprContext* ctx, NameToken const& op_name,
                             ast::CallArgs const& args) {
  if (args.rest()) {
    return status::InvalidArgumentError(
        absl::StrFormat("Comparison operator '%s' cannot take a rest argument.",
                        op_name.value()));
  }

  absl::Span<ast::Expr const> op_args = args.args();
  if (op_args.size() < 2) {
    return status::InvalidArgumentError(absl::StrFormat(
        "Comparison operator '%s' must take at least two arguments.",
        op_name.value()));
  }

  auto done = ctx->func_builder()->CreateLabelRef();

  RETURN_IF_ERROR(ctx->BuildExpr(op_args[0]));
  ctx->func_builder()->AddPushOp();
  RETURN_IF_ERROR(ctx->BuildExpr(op_args[1]));
  ctx->func_builder()->AddBinOp(Op);

  for (auto const& arg : op_args.subspan(2)) {
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &done);
    ctx->func_builder()->AddPushPrevOp();
    RETURN_IF_ERROR(ctx->BuildExpr(arg));
    ctx->func_builder()->AddBinOp(Op);
  }
  ctx->func_builder()->AddLabel(&done);
  return status::OkStatus();
}

using CallFunc = status::Status (*)(ExprContext* ctx, NameToken const& op_name,
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
      {"<", &BuildCompExpr<FunctionBuilder::LT>},
      {"<=", &BuildCompExpr<FunctionBuilder::LE>},
      {">", &BuildCompExpr<FunctionBuilder::GT>},
      {">=", &BuildCompExpr<FunctionBuilder::GE>},
      {"==", &BuildCompExpr<FunctionBuilder::EQ>},
      {"!=", &BuildCompExpr<FunctionBuilder::NE>},  // Is NE also n-ary?
      {"u<", &BuildCompExpr<FunctionBuilder::ULT>},
      {"u<=", &BuildCompExpr<FunctionBuilder::ULE>},
      {"u>", &BuildCompExpr<FunctionBuilder::UGT>},
      {"u>=", &BuildCompExpr<FunctionBuilder::UGE>},
      {"+", &BuildMultiExpr<FunctionBuilder::ADD>},
      {"*", &BuildMultiExpr<FunctionBuilder::MUL>},
      {"|", &BuildMultiExpr<FunctionBuilder::OR>},
      {"&", &BuildMultiExpr<FunctionBuilder::AND>},
      {"^", &BuildMultiExpr<FunctionBuilder::XOR>},
      {"and", &BuildAndExpr},
      {"or", &BuildOrExpr},
  };
  return builtins;
}

status::Status BuildCallExpr(ExprContext* ctx, ast::CallExpr const& call) {
  ASSIGN_OR_RETURN(auto num_args, BuildCallArgs(ctx, call.call_args()));
  auto const& target = call.target();
  // The original appears to only support calls to names, but I think
  // there's a reason that I made this support general expressions.
  if (!target.has<ast::VarExpr>()) {
    return status::InvalidArgumentError(
        "Only calls to names are supported at this time.");
  }
  auto const& target_name = target.as<ast::VarExpr>().name();

  // A number of operations are represented as calls to built-in functions.
  auto const& builtins = GetCallBuiltins();
  auto it = builtins.find(target_name.value());
  if (it != builtins.end()) {
    return it->second(ctx, target_name, call.call_args());
  }

  ASSIGN_OR_RETURN(auto proc, ctx->LookupProc(target_name.value()));
  return proc.visit(
      [&](ExprEnvironment::LocalProc const& local) -> status::Status {
        ctx->func_builder()->AddProcCall(std::string(local.name.value()),
                                         num_args, local.proc_ref);
        return status::OkStatus();
      },
      [&](ExprEnvironment::ExternProc const& ext) -> status::Status {
        ctx->func_builder()->AddExternCall(std::string(ext.name.value()),
                                           num_args, ext.script_num.value(),
                                           ext.extern_offset);
        return status::OkStatus();
      },
      [&](ExprEnvironment::KernelProc const& kernel) -> status::Status {
        ctx->func_builder()->AddKernelCall(std::string(kernel.name.value()),
                                           num_args, kernel.kernel_offset);
        return status::OkStatus();
      });
}

status::StatusOr<std::size_t> BuildSendClause(ExprContext* ctx,
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

  auto symbol = ctx->LookupSym(sel_and_args.sel_name.value());

  if (!symbol.ok() && !status::IsNotFound(symbol.status())) {
    return std::move(symbol).status();
  }

  if (symbol.ok() && symbol->has<ExprEnvironment::VarSym>()) {
    // Not maximally efficient, but it should work.
    RETURN_IF_ERROR(BuildVarLoadExpr(ctx, FunctionBuilder::LOAD,
                                     sel_and_args.sel_name, nullptr));
    ctx->func_builder()->AddPushOp();
  } else {
    auto selector = ctx->LookupSelector(sel_and_args.sel_name.value());
    if (!selector) {
      return status::NotFoundError(absl::StrFormat(
          "Selector '%s' not found.", sel_and_args.sel_name.value()));
    }
    ctx->func_builder()->AddPushImmediate(int(selector->value()));
  }

  if (sel_and_args.args) {
    ASSIGN_OR_RETURN(auto num_args, BuildCallArgs(ctx, *sel_and_args.args));
    return num_args + 1;
  } else {
    ctx->func_builder()->AddPushImmediate(0);
    return 2UL;
  }
}

status::Status BuildSendExpr(ExprContext* ctx, ast::SendExpr const& send) {
  std::size_t num_args = 0;
  for (auto const& clause : send.clauses()) {
    ASSIGN_OR_RETURN(auto clause_num_args, BuildSendClause(ctx, clause));
    num_args += clause_num_args;
  }

  return send.target().visit(
      [&](ast::SelfSendTarget const& self_target) {
        ctx->func_builder()->AddSelfSend(num_args);
        return status::OkStatus();
      },
      [&](ast::SuperSendTarget const& index_expr) -> status::Status {
        if (!ctx->super_info()) {
          return status::FailedPreconditionError(
              "Cannot send to super without a super class.");
        }
        auto super_info = *ctx->super_info();
        ctx->func_builder()->AddSuperSend(
            std::string(super_info.super_name.value()), num_args,
            int(super_info.species.value()));
        return status::OkStatus();
      },
      [&](ast::ExprSendTarget const& expr) -> status::Status {
        RETURN_IF_ERROR(ctx->BuildExpr(expr.target()));
        ctx->func_builder()->AddSend(num_args);
        return status::OkStatus();
      });
}

status::Status BuildAssignExpr(ExprContext* ctx,
                               ast::AssignExpr const& assign) {
  auto assign_op = BuildAssignOp(assign.kind());
  auto result = GetVarNameAndIndex(assign.target());
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
  return status::OkStatus();
}

status::Status BuildIncDecExpr(ExprContext* ctx,
                               ast::IncDecExpr const& inc_dec) {
  auto inc_dec_op = inc_dec.kind() == ast::IncDecExpr::INC
                        ? FunctionBuilder::INC
                        : FunctionBuilder::DEC;
  auto result = GetVarNameAndIndex(inc_dec.target());
  RETURN_IF_ERROR(
      BuildVarLoadExpr(ctx, inc_dec_op, result.var_name, result.index));
  return status::OkStatus();
}

status::Status BuildExprList(ExprContext* ctx, ast::ExprList const& expr_list) {
  for (auto const& expr : expr_list.exprs()) {
    RETURN_IF_ERROR(ctx->BuildExpr(expr));
  }
  return status::OkStatus();
}

status::Status BuildBreakExpr(ExprContext* ctx,
                              ast::BreakExpr const& break_expr) {
  std::size_t level = 0;
  if (break_expr.level()) {
    level = break_expr.level()->value();
  }
  auto* break_label = ctx->GetBreakLabel(level);
  if (!break_label) {
    return status::FailedPreconditionError(
        "Cannot break to level without a loop.");
  }
  if (break_expr.condition()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**break_expr.condition()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, break_label);
  } else {
    ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, break_label);
  }
  return status::OkStatus();
}

status::Status BuildContEpxr(ExprContext* ctx,
                             ast::ContinueExpr const& break_expr) {
  std::size_t level = 0;
  if (break_expr.level()) {
    level = break_expr.level()->value();
  }
  auto* break_label = ctx->GetContLabel(level);
  if (!break_label) {
    return status::FailedPreconditionError(
        "Cannot break to level without a loop.");
  }
  if (break_expr.condition()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**break_expr.condition()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT, break_label);
  } else {
    ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, break_label);
  }
  return status::OkStatus();
}

status::Status BuildIfExpr(ExprContext* ctx, ast::IfExpr const& if_expr) {
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
  return status::OkStatus();
}

status::Status BuildCondExpr(ExprContext* ctx, ast::CondExpr const& cond_expr) {
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
  return status::OkStatus();
}

status::Status BuildSwitchExpr(ExprContext* ctx,
                               ast::SwitchExpr const& switch_expr) {
  auto done = ctx->func_builder()->CreateLabelRef();
  RETURN_IF_ERROR(ctx->BuildExpr(switch_expr.switch_expr()));
  ctx->func_builder()->AddPushOp();
  for (std::size_t i = 0; i < switch_expr.cases().size(); ++i) {
    auto next = ctx->func_builder()->CreateLabelRef();
    auto const& branch = switch_expr.cases()[i];
    bool at_end =
        (i == switch_expr.cases().size() - 1) && !switch_expr.else_case();
    ctx->func_builder()->AddDupOp();
    RETURN_IF_ERROR(BuildConstExpr(ctx, branch.value));
    ctx->func_builder()->AddBinOp(FunctionBuilder::EQ);
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT,
                                     at_end ? &done : &next);
    RETURN_IF_ERROR(ctx->BuildExpr(*branch.body));
    if (!at_end) {
      ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &done);
      ctx->func_builder()->AddLabel(&next);
    }
  }
  if (switch_expr.else_case()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**switch_expr.else_case()));
  }
  ctx->func_builder()->AddLabel(&done);
  return status::OkStatus();
}

status::Status BuildSwitchToExpr(ExprContext* ctx,
                                 ast::SwitchToExpr const& switch_expr) {
  auto done = ctx->func_builder()->CreateLabelRef();
  RETURN_IF_ERROR(ctx->BuildExpr(switch_expr.switch_expr()));
  ctx->func_builder()->AddPushOp();
  for (std::size_t i = 0; i < switch_expr.cases().size(); ++i) {
    auto next = ctx->func_builder()->CreateLabelRef();
    auto const& branch = switch_expr.cases()[i];
    bool at_end =
        (i == switch_expr.cases().size() - 1) && !switch_expr.else_case();
    ctx->func_builder()->AddDupOp();
    ctx->func_builder()->AddLoadImmediate(int(i));
    ctx->func_builder()->AddBinOp(FunctionBuilder::EQ);
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BT,
                                     at_end ? &done : &next);
    RETURN_IF_ERROR(ctx->BuildExpr(branch));
    if (!at_end) {
      ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &done);
      ctx->func_builder()->AddLabel(&next);
    }
  }
  if (switch_expr.else_case()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**switch_expr.else_case()));
  }
  ctx->func_builder()->AddLabel(&done);
  return status::OkStatus();
}

status::Status BuildWhileExpr(ExprContext* ctx,
                              ast::WhileExpr const& while_expr) {
  auto start = ctx->func_builder()->CreateLabelRef();
  auto done = ctx->func_builder()->CreateLabelRef();

  Loop loop(ctx, &start, &done);

  ctx->func_builder()->AddLabel(&start);
  if (while_expr.condition()) {
    RETURN_IF_ERROR(ctx->BuildExpr(**while_expr.condition()));
    ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &done);
  }
  RETURN_IF_ERROR(ctx->BuildExpr(while_expr.body()));
  ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &start);
  ctx->func_builder()->AddLabel(&done);
  return status::OkStatus();
}

status::Status BuildForExpr(ExprContext* ctx, ast::ForExpr const& for_expr) {
  auto next = ctx->func_builder()->CreateLabelRef();
  auto done = ctx->func_builder()->CreateLabelRef();

  // General approach:
  //
  //   <init>
  // cond:
  //   <condition>
  //   bnt done
  //   <body>
  // next:
  //   <update>
  //   jmp cond
  // done:

  auto cond = ctx->func_builder()->CreateLabelRef();
  RETURN_IF_ERROR(ctx->BuildExpr(for_expr.init()));
  ctx->func_builder()->AddLabel(&cond);
  RETURN_IF_ERROR(ctx->BuildExpr(for_expr.condition()));
  ctx->func_builder()->AddBranchOp(FunctionBuilder::BNT, &done);
  {
    Loop loop(ctx, &next, &done);
    RETURN_IF_ERROR(ctx->BuildExpr(for_expr.body()));
  }
  ctx->func_builder()->AddLabel(&next);
  RETURN_IF_ERROR(ctx->BuildExpr(for_expr.update()));
  ctx->func_builder()->AddBranchOp(FunctionBuilder::JMP, &cond);
  ctx->func_builder()->AddLabel(&done);
  return status::OkStatus();
}

class ExprContextImpl : public ExprContext {
 public:
  using ExprContext::ExprContext;

  status::Status BuildExpr(ast::Expr const& expr) override {
    return expr.visit(
        [&](ast::AddrOfExpr const& binary) {
          return BuildAddrOfExpr(this, binary).WithLocation();
        },
        [&](ast::SelectLitExpr const& select_lit) {
          return BuildSelectLitExpr(this, select_lit).WithLocation();
        },
        [&](ast::ConstValueExpr const& const_expr) {
          return BuildConstExpr(this, const_expr.value()).WithLocation();
        },
        [&](ast::VarExpr const& var_ref) {
          return BuildVarLoadExpr(this, FunctionBuilder::LOAD, var_ref.name(),
                                  nullptr)
              .WithLocation();
        },
        [&](ast::ArrayIndexExpr const& array_index) {
          return BuildVarLoadExpr(this, FunctionBuilder::LOAD,
                                  array_index.var_name(), &array_index.index())
              .WithLocation();
        },
        [&](ast::CallExpr const& selector_ref) {
          return BuildCallExpr(this, selector_ref).WithLocation();
        },
        [&](ast::ReturnExpr const& array_ref) {
          if (array_ref.expr()) {
            RETURN_IF_ERROR(BuildExpr(**array_ref.expr()));
          }
          func_builder()->AddReturnOp();
          return status::OkStatus();
        },
        [&](ast::BreakExpr const& break_expr) {
          return BuildBreakExpr(this, break_expr).WithLocation();
        },
        [&](ast::ContinueExpr const& cont_expr) {
          return BuildContEpxr(this, cont_expr).WithLocation();
        },
        [&](ast::WhileExpr const& while_expr) {
          return BuildWhileExpr(this, while_expr).WithLocation();
        },
        [&](ast::ForExpr const& for_expr) {
          return BuildForExpr(this, for_expr).WithLocation();
        },
        [&](ast::IfExpr const& if_expr) {
          return BuildIfExpr(this, if_expr).WithLocation();
        },
        [&](ast::CondExpr const& cond_expr) {
          return BuildCondExpr(this, cond_expr).WithLocation();
        },
        [&](ast::SwitchExpr const& switch_expr) {
          return BuildSwitchExpr(this, switch_expr).WithLocation();
        },
        [&](ast::SwitchToExpr const& switch_to_expr) {
          return BuildSwitchToExpr(this, switch_to_expr).WithLocation();
        },
        [&](ast::SendExpr const& send_expr) {
          return BuildSendExpr(this, send_expr).WithLocation();
        },
        [&](ast::AssignExpr const& assign_expr) {
          return BuildAssignExpr(this, assign_expr).WithLocation();
        },
        [&](ast::IncDecExpr const& inc_dec_expr) {
          return BuildIncDecExpr(this, inc_dec_expr).WithLocation();
        },
        [&](ast::ExprList const& expr_list) {
          return BuildExprList(this, expr_list).WithLocation();
        });
  }
};

}  // namespace

std::unique_ptr<ExprContext> CreateExprContext(
    ExprEnvironment const* expr_env, codegen::CodeGenerator* codegen,
    codegen::FunctionBuilder* func_builder) {
  return std::make_unique<ExprContextImpl>(expr_env, codegen, func_builder);
}

}  // namespace sem