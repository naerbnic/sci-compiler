#ifndef SEM_EXPR_CONTEXT_HPP
#define SEM_EXPR_CONTEXT_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/parsers/sci/ast.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/sem/property_list.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"

namespace sem {

class Loop;

class ExprEnvironment {
 public:
  struct ParamSym {
    std::size_t param_offset;
  };

  struct TempSym {
    std::size_t temp_offset;
  };

  struct GlobalSym {
    std::size_t global_offset;
  };

  struct LocalSym {
    std::size_t local_offset;
  };

  struct VarSym : public util::ChoiceBase<VarSym, ParamSym, GlobalSym, LocalSym,
                                          TempSym> {
    using ChoiceBase::ChoiceBase;
  };

  struct PropSym {
    std::size_t prop_offset;
    SelectorTable::Entry const* selector;
  };

  struct Sym : public util::ChoiceBase<Sym, PropSym, VarSym> {
    using ChoiceBase::ChoiceBase;
  };

  struct LocalProc {
    NameToken name;
    codegen::PtrRef* proc_ref;
  };

  struct ExternProc {
    NameToken name;
    ScriptNum script_num;
    std::size_t extern_offset;
  };

  struct KernelProc {
    NameToken name;
    std::size_t kernel_offset;
  };

  struct Proc
      : public util::ChoiceBase<Proc, LocalProc, ExternProc, KernelProc> {
    using ChoiceBase::ChoiceBase;
  };

  struct SuperInfo {
    ClassSpecies species;
    NameToken super_name;
  };

  static std::unique_ptr<ExprEnvironment> Create(
      ModuleEnvironment const* mod_env, PropertyList const* prop_list,
      std::optional<SuperInfo> super_info,
      std::map<util::RefStr, ParamSym, std::less<>> proc_local_table,
      std::map<util::RefStr, TempSym, std::less<>> proc_temp_table);

  virtual ~ExprEnvironment() = default;

  virtual std::optional<SuperInfo> GetSuperInfo() const = 0;

  virtual std::optional<SelectorNum> LookupSelector(
      std::string_view name) const = 0;

  virtual absl::StatusOr<Sym> LookupSym(std::string_view name) const = 0;

  virtual absl::StatusOr<Proc> LookupProc(std::string_view name) const = 0;
};

class ExprContext {
 public:
  ExprContext(ExprEnvironment const* expr_env, codegen::CodeGenerator* codegen,
              codegen::FunctionBuilder* func_builder)
      : expr_env_(expr_env), codegen_(codegen), func_builder_(func_builder) {}

  virtual ~ExprContext() = default;

  codegen::CodeGenerator* codegen() const { return codegen_; }
  codegen::FunctionBuilder* func_builder() const { return func_builder_; }
  std::optional<ExprEnvironment::SuperInfo> super_info() const {
    return expr_env_->GetSuperInfo();
  }

  absl::StatusOr<ExprEnvironment::Sym> LookupSym(std::string_view name) const {
    return expr_env_->LookupSym(name);
  }

  absl::StatusOr<ExprEnvironment::Proc> LookupProc(
      std::string_view name) const {
    return expr_env_->LookupProc(name);
  }

  std::optional<SelectorNum> LookupSelector(std::string_view name) const {
    return expr_env_->LookupSelector(name);
  }

  codegen::LabelRef* GetContLabel(std::size_t num_levels) const;
  codegen::LabelRef* GetBreakLabel(std::size_t num_levels) const;

  virtual absl::Status BuildExpr(ast::Expr const& expr) const = 0;

 private:
  friend class Loop;

  Loop const* FindLoop(std::size_t at_level) const;

  ExprEnvironment const* expr_env_;
  codegen::CodeGenerator* codegen_;
  codegen::FunctionBuilder* func_builder_;
  mutable Loop const* loop_ = nullptr;
};

class Loop {
 public:
  explicit Loop(ExprContext const* ctx, codegen::LabelRef* cont_label,
                codegen::LabelRef* break_label)
      : ctx_(ctx),
        prev_(ctx->loop_),
        cont_label_(cont_label),
        break_label_(break_label) {
    ctx_->loop_ = this;
  }

  Loop const* prev() const { return prev_; }
  codegen::LabelRef* cont_label() const { return cont_label_; }
  codegen::LabelRef* break_label() const { return break_label_; }

  ~Loop() noexcept(false) {
    if (ctx_->loop_ != this) {
      throw std::logic_error("Loop destructor called in non-nesting order");
    }
    ctx_->loop_ = prev_;
  }

 private:
  ExprContext const* ctx_;
  Loop const* prev_;
  codegen::LabelRef* cont_label_;
  codegen::LabelRef* break_label_;
};

inline Loop const* ExprContext::FindLoop(std::size_t at_level) const {
  auto const* curr_loop = loop_;
  while (at_level > 0) {
    at_level--;
    if (curr_loop->prev() == nullptr) {
      return curr_loop;
    }
  }
  return curr_loop;
}

inline codegen::LabelRef* ExprContext::GetContLabel(
    std::size_t at_level) const {
  auto const* loop = FindLoop(at_level);
  if (!loop) {
    return nullptr;
  }
  return loop->cont_label();
}

inline codegen::LabelRef* ExprContext::GetBreakLabel(
    std::size_t at_level) const {
  auto const* loop = FindLoop(at_level);
  if (!loop) {
    return nullptr;
  }
  return loop->break_label();
}

}  // namespace sem

#endif