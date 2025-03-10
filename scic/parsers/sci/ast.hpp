#ifndef SCIC_PARSERS_SCI_AST_HPP
#define SCIC_PARSERS_SCI_AST_HPP

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token_source.hpp"
#include "util/io/printer.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"
#include "util/types/sequence.hpp"

namespace parsers::sci {

namespace internal {

template <class T>
using PtrElementType = std::remove_reference_t<decltype(*std::declval<T&>())>;

template <class T>
concept IsSmartPtr = requires(T& t) {
  typename PtrElementType<T>;
  { std::to_address(t) } -> std::convertible_to<PtrElementType<T>*>;
  { t.operator->() } -> std::convertible_to<PtrElementType<T>*>;
  { *t } -> std::convertible_to<PtrElementType<T>&>;
  { t.get() } -> std::convertible_to<PtrElementType<T>*>;
} && requires(T const& t) {
  typename PtrElementType<T const>;
  { std::to_address(t) } -> std::convertible_to<PtrElementType<T const>*>;
  { t.operator->() } -> std::convertible_to<PtrElementType<T const>*>;
  { *t } -> std::convertible_to<PtrElementType<T const>&>;
  { t.get() } -> std::convertible_to<PtrElementType<T const>*>;
};

template <class T>
concept IsPtr = IsSmartPtr<T> || std::is_pointer_v<T>;

template <class T>
struct NodeElementType {
  using type = T;
};

template <class T>
  requires IsPtr<T>
struct NodeElementType<T> {
  using type = typename std::pointer_traits<T>::element_type;
};

}  // namespace internal

// A value that carries provenance information about which file it comes
// from. This can be used to provide better error messages.
template <class T>
class TokenNode {
 public:
  using element_type = internal::NodeElementType<T>::type;
  TokenNode(T value, tokens::TokenSource token_source)
      : value_(std::move(value)), token_source_(std::move(token_source)) {}

  TokenNode(TokenNode const&) = default;
  TokenNode(TokenNode&&) = default;
  TokenNode& operator=(TokenNode const&) = default;
  TokenNode& operator=(TokenNode&&) = default;

  template <class U>
    requires(std::convertible_to<U, T> && !std::same_as<U, T>)
  TokenNode(TokenNode<U> other)
      : value_(std::move(other).value()),
        token_source_(std::move(other).token_source()) {}

  T const& value() const& { return value_; }
  T&& value() && { return std::move(value_); }
  tokens::TokenSource const& token_source() const { return token_source_; }
  text::TextRange const& text_range() const {
    return token_source_.sources()[0];
  }

  // Act as a smart pointer to the value. If the internal type is a pointer,
  // act as if it were transparent.
  explicit operator bool() const { return bool(get_ptr()); }
  element_type* operator->() { return get_ptr(); }
  element_type const* operator->() const { return get_ptr(); }
  element_type& operator*() { return *get_ptr(); }
  element_type const& operator*() const { return *get_ptr(); }
  element_type* get() { return get_ptr(); }
  element_type const* get() const { return get_ptr(); }

 private:
  template <class U = T>
    requires internal::IsPtr<U>
  element_type* get_ptr() {
    return std::to_address(value_);
  }

  template <class U = T>
    requires internal::IsPtr<U>
  element_type const* get_ptr() const {
    return std::to_address(value_);
  }

  template <class U = T>
    requires(!internal::IsPtr<U>)
  element_type* get_ptr() {
    return &value_;
  }

  template <class U = T>
    requires(!internal::IsPtr<U>)
  element_type const* get_ptr() const {
    return &value_;
  }

  T value_;
  tokens::TokenSource token_source_;

  DEFINE_PRINTERS(TokenNode, "value", value_, "text_range", token_source_);
};

// AST Nodes for the SCI language parse tree.

// Variable Definitions. This can be either a single variable, or
// an array definition.

class SingleVarDef {
 public:
  SingleVarDef(TokenNode<util::RefStr> name) : name_(std::move(name)) {}

  TokenNode<util::RefStr> const& name() const { return name_; }

 private:
  TokenNode<util::RefStr> name_;

  DEFINE_PRINTERS(SingleVarDef, "name", name_);
};

class ArrayVarDef {
 public:
  ArrayVarDef(TokenNode<util::RefStr> name, TokenNode<int> size)
      : name_(std::move(name)), size_(std::move(size)) {}

  TokenNode<util::RefStr> const& name() const { return name_; }
  TokenNode<int> const& size() const { return size_; }

 private:
  TokenNode<util::RefStr> name_;
  TokenNode<int> size_;

  DEFINE_PRINTERS(ArrayVarDef, "name", name_, "size", size_);
};

class VarDef : public util::ChoiceBase<VarDef, SingleVarDef, ArrayVarDef> {
  using ChoiceBase::ChoiceBase;
};

class NumConstValue {
 public:
  NumConstValue(TokenNode<int> value) : value_(std::move(value)) {}

  TokenNode<int> const& value() const { return value_; }

 private:
  TokenNode<int> value_;

  DEFINE_PRINTERS(NumConstValue, "value", value_);
};

class StringConstValue {
 public:
  StringConstValue(TokenNode<util::RefStr> value) : value_(std::move(value)) {}

  TokenNode<util::RefStr> const& value() const { return value_; }

 private:
  TokenNode<util::RefStr> value_;
  DEFINE_PRINTERS(StringConstValue, "value", value_);
};

class ConstValue
    : public util::ChoiceBase<ConstValue, NumConstValue, StringConstValue> {
  using ChoiceBase::ChoiceBase;
};

// Initial values

class ArrayInitialValue {
 public:
  ArrayInitialValue(std::vector<ConstValue> value) : value_(std::move(value)) {}

  std::vector<ConstValue> const& value() const { return value_; }

 private:
  std::vector<ConstValue> value_;

  DEFINE_PRINTERS(ArrayInitialValue, "value", value_);
};

class InitialValue
    : public util::ChoiceBase<InitialValue, ConstValue, ArrayInitialValue> {
  using ChoiceBase::ChoiceBase;
};

// Expr Definitions

class Expr;
class LValueExpr;

class CallArgs {
 public:
  struct Rest {
    std::optional<TokenNode<util::RefStr>> rest_var;
  };

  explicit CallArgs(std::vector<Expr> args, std::optional<Rest>);
  ~CallArgs();

  // All of these are defaulted, but we need to define them in the .cpp file
  // to avoid an incomplete type issue.
  CallArgs(CallArgs const&) = delete;
  CallArgs(CallArgs&&);
  CallArgs& operator=(CallArgs const&) = delete;
  CallArgs& operator=(CallArgs&&);

  std::vector<Expr> const& args() const { return args_; }
  std::optional<Rest> const& rest() const { return rest_; }

 private:
  std::vector<Expr> args_;
  std::optional<Rest> rest_;

  DEFINE_PRINTERS(CallArgs, "args", args_, "rest", rest_);
};

class AddrOfExpr {
 public:
  AddrOfExpr(std::unique_ptr<LValueExpr> expr) : expr_(std::move(expr)) {}

  LValueExpr const& expr() const { return *expr_; }

 private:
  std::unique_ptr<LValueExpr> expr_;

  DEFINE_PRINTERS(AddrOfExpr, "expr", expr_);
};

class SelectLitExpr {
 public:
  SelectLitExpr(TokenNode<util::RefStr> selector)
      : selector_(std::move(selector)) {}

  TokenNode<util::RefStr> const& selector() const { return selector_; }

 private:
  TokenNode<util::RefStr> selector_;

  DEFINE_PRINTERS(SelectLitExpr, "selector", selector_);
};

// A plain variable reference.
class VarExpr {
 public:
  VarExpr(TokenNode<util::RefStr> name) : name_(std::move(name)) {}

  TokenNode<util::RefStr> const& name() const { return name_; }

 private:
  TokenNode<util::RefStr> name_;

  DEFINE_PRINTERS(VarExpr, "name", name_);
};

class ArrayIndexExpr {
 public:
  ArrayIndexExpr(TokenNode<util::RefStr> var_name, std::unique_ptr<Expr> index)
      : var_name_(std::move(var_name)), index_(std::move(index)) {}

  TokenNode<util::RefStr> const& var_name() const { return var_name_; }
  Expr const& index() const { return *index_; }

 private:
  TokenNode<util::RefStr> var_name_;
  std::unique_ptr<Expr> index_;

  DEFINE_PRINTERS(ArrayIndexExpr, "var_name", var_name_, "index", index_);
};

class ConstValueExpr {
 public:
  ConstValueExpr(ConstValue value) : value_(std::move(value)) {}

  ConstValue const& value() const { return value_; }

 private:
  ConstValue value_;

  DEFINE_PRINTERS(ConstValueExpr, "value", value_);
};

// A call expresion, of (<name> <arg> ...). Aside from control flow
// structures, and send expressions, other expressions of that form
// are represented as calls.
class CallExpr {
 public:
  CallExpr(std::unique_ptr<Expr> target, CallArgs call_args)
      : target_(std::move(target)), call_args_(std::move(call_args)) {}

  Expr const& target() const { return *target_; }
  CallArgs const& call_args() const { return call_args_; }

 private:
  std::unique_ptr<Expr> target_;
  CallArgs call_args_;

  DEFINE_PRINTERS(CallExpr, "target", target_, "call_args", call_args_);
};

class ReturnExpr {
 public:
  ReturnExpr(std::optional<std::unique_ptr<Expr>> expr)
      : expr_(std::move(expr)) {}

  std::optional<std::unique_ptr<Expr>> const& expr() const { return expr_; }

 private:
  std::optional<std::unique_ptr<Expr>> expr_;

  DEFINE_PRINTERS(ReturnExpr, "expr", expr_);
};

class BreakExpr {
 public:
  BreakExpr(std::optional<std::unique_ptr<Expr>> condition,
            std::optional<TokenNode<int>> level)
      : condition_(std::move(condition)), level_(std::move(level)) {}

  std::optional<std::unique_ptr<Expr>> const& condition() const {
    return condition_;
  }
  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<std::unique_ptr<Expr>> condition_;
  std::optional<TokenNode<int>> level_;

  DEFINE_PRINTERS(BreakExpr, "condition", condition_, "level", level_);
};

class ContinueExpr {
 public:
  ContinueExpr(std::optional<std::unique_ptr<Expr>> condition,
               std::optional<TokenNode<int>> level)
      : condition_(std::move(condition)), level_(std::move(level)) {}

  std::optional<std::unique_ptr<Expr>> const& condition() const {
    return condition_;
  }
  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<std::unique_ptr<Expr>> condition_;
  std::optional<TokenNode<int>> level_;

  DEFINE_PRINTERS(ContinueExpr, "condition", condition_, "level", level_);
};

// A while expression. If this is a repeat loop, the condition will be
// std::nullopt.
class WhileExpr {
 public:
  WhileExpr(std::optional<std::unique_ptr<Expr>> condition,
            std::unique_ptr<Expr> body)
      : condition_(std::move(condition)), body_(std::move(body)) {}

  std::optional<std::unique_ptr<Expr>> const& condition() const {
    return condition_;
  }
  Expr const& body() const { return *body_; }

 private:
  std::optional<std::unique_ptr<Expr>> condition_;
  std::unique_ptr<Expr> body_;

  DEFINE_PRINTERS(WhileExpr, "condition", condition_, "body", body_);
};

class ForExpr {
 public:
  ForExpr(std::unique_ptr<Expr> init, std::unique_ptr<Expr> condition,
          std::unique_ptr<Expr> update, std::unique_ptr<Expr> body)
      : init_(std::move(init)),
        condition_(std::move(condition)),
        update_(std::move(update)),
        body_(std::move(body)) {}

  Expr const& init() const { return *init_; }
  Expr const& condition() const { return *condition_; }
  Expr const& update() const { return *update_; }
  Expr const& body() const { return *body_; }

 private:
  std::unique_ptr<Expr> init_;
  std::unique_ptr<Expr> condition_;
  std::unique_ptr<Expr> update_;
  std::unique_ptr<Expr> body_;

  DEFINE_PRINTERS(ForExpr, "init", init_, "condition", condition_, "update",
                  update_, "body", body_);
};

class IfExpr {
 public:
  IfExpr(std::unique_ptr<Expr> condition, std::unique_ptr<Expr> then_body,
         std::optional<std::unique_ptr<Expr>> else_body)
      : condition_(std::move(condition)),
        then_body_(std::move(then_body)),
        else_body_(std::move(else_body)) {}

  Expr const& condition() const { return *condition_; }
  Expr const& then_body() const { return *then_body_; }
  std::optional<std::unique_ptr<Expr>> const& else_body() const {
    return else_body_;
  }

 private:
  std::unique_ptr<Expr> condition_;
  std::unique_ptr<Expr> then_body_;
  std::optional<std::unique_ptr<Expr>> else_body_;

  DEFINE_PRINTERS(IfExpr, "condition", condition_, "then_body", then_body_,
                  "else_body", else_body_);
};

class CondExpr {
 public:
  struct Branch {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> body;
  };

  CondExpr(std::vector<Branch> branches,
           std::optional<std::unique_ptr<Expr>> else_body)
      : branches_(std::move(branches)), else_body_(std::move(else_body)) {}

  std::vector<Branch> const& branches() const { return branches_; }
  std::optional<std::unique_ptr<Expr>> const& else_body() const {
    return else_body_;
  }

 private:
  std::vector<Branch> branches_;
  std::optional<std::unique_ptr<Expr>> else_body_;

  DEFINE_PRINTERS(CondExpr, "branches", branches_, "else_body", else_body_);
};

class SwitchExpr {
 public:
  struct Case {
    ConstValue value;
    std::unique_ptr<Expr> body;
  };

  SwitchExpr(std::unique_ptr<Expr> switch_expr, std::vector<Case> cases,
             std::optional<std::unique_ptr<Expr>> else_case)
      : switch_expr_(std::move(switch_expr)),
        cases_(std::move(cases)),
        else_case_(std::move(else_case)) {}

  Expr const& switch_expr() const { return *switch_expr_; }
  std::vector<Case> const& cases() const { return cases_; }
  std::optional<std::unique_ptr<Expr>> const& else_case() const {
    return else_case_;
  }

 private:
  std::unique_ptr<Expr> switch_expr_;
  std::vector<Case> cases_;
  std::optional<std::unique_ptr<Expr>> else_case_;

  DEFINE_PRINTERS(SwitchExpr, "switch_expr", switch_expr_, "cases", cases_,
                  "else_case", else_case_);
};

class SwitchToExpr {
 public:
  SwitchToExpr(std::unique_ptr<Expr> switch_expr,
               std::vector<std::unique_ptr<Expr>> cases,
               std::optional<std::unique_ptr<Expr>> else_case)
      : switch_expr_(std::move(switch_expr)),
        cases_(std::move(cases)),
        else_case_(std::move(else_case)) {}

  Expr const& switch_expr() const { return *switch_expr_; }
  util::SeqView<Expr const> cases() const {
    return util::SeqView<Expr const>::Deref(cases_);
  }

  std::optional<std::unique_ptr<Expr>> const& else_case() const {
    return else_case_;
  }

 private:
  std::unique_ptr<Expr> switch_expr_;
  std::vector<std::unique_ptr<Expr>> cases_;
  std::optional<std::unique_ptr<Expr>> else_case_;

  DEFINE_PRINTERS(SwitchToExpr, "switch_expr", switch_expr_, "cases", cases_,
                  "else_case", else_case_);
};

class IncDecExpr {
 public:
  enum Kind {
    INC,
    DEC,
  };

  IncDecExpr(Kind kind, std::unique_ptr<LValueExpr> target)
      : kind_(kind), target_(std::move(target)) {}

  Kind kind() const { return kind_; }
  LValueExpr const& target() const { return *target_; }

 private:
  Kind kind_;
  std::unique_ptr<LValueExpr> target_;
};

class SelfSendTarget {
 private:
  DEFINE_PRINTERS(SelfSendTarget);
};
class SuperSendTarget {
 private:
  DEFINE_PRINTERS(SuperSendTarget);
};
class ExprSendTarget {
 public:
  explicit ExprSendTarget(std::unique_ptr<Expr> target)
      : target_(std::move(target)) {}

  Expr const& target() const { return *target_; }

 private:
  std::unique_ptr<Expr> target_;

  DEFINE_PRINTERS(ExprSendTarget, "target", target_);
};

class SendTarget
    : public util::ChoiceBase<SendTarget,  //
                              SelfSendTarget, SuperSendTarget, ExprSendTarget> {
  using ChoiceBase::ChoiceBase;
};

class PropReadSendClause {
 public:
  explicit PropReadSendClause(TokenNode<util::RefStr> prop_name)
      : prop_name_(std::move(prop_name)) {}

  TokenNode<util::RefStr> const& prop_name() const { return prop_name_; }

 private:
  TokenNode<util::RefStr> prop_name_;

  DEFINE_PRINTERS(PropReadSendClause, "prop_name", prop_name_);
};

class MethodSendClause {
 public:
  MethodSendClause(TokenNode<util::RefStr> method_name, CallArgs call_args)
      : selector_(std::move(method_name)), call_args_(std::move(call_args)) {}

  TokenNode<util::RefStr> const& selector() const { return selector_; }
  CallArgs const& call_args() const { return call_args_; }

 private:
  TokenNode<util::RefStr> selector_;
  CallArgs call_args_;

  DEFINE_PRINTERS(MethodSendClause, "selector", selector_, "call_args",
                  call_args_);
};

class SendClause
    : public util::ChoiceBase<SendClause,  //
                              PropReadSendClause, MethodSendClause> {
  using ChoiceBase::ChoiceBase;
};

class SendExpr {
 public:
  SendExpr(SendTarget target, std::vector<SendClause> clauses)
      : target_(std::move(target)), clauses_(std::move(clauses)) {}

  SendTarget const& target() const { return target_; }
  std::vector<SendClause> const& clauses() const { return clauses_; }

 private:
  SendTarget target_;
  std::vector<SendClause> clauses_;

  DEFINE_PRINTERS(SendExpr, "target", target_, "clauses", clauses_);
};

class AssignExpr {
 public:
  enum Kind {
    DIRECT,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    AND,
    OR,
    XOR,
    SHR,
    SHL,
  };
  AssignExpr(Kind kind, std::unique_ptr<LValueExpr> target,
             std::unique_ptr<Expr> value)
      : kind_(kind), target_(std::move(target)), value_(std::move(value)) {}

  Kind kind() const { return kind_; }
  LValueExpr const& target() const { return *target_; }
  Expr const& value() const { return *value_; }

 private:
  Kind kind_;
  std::unique_ptr<LValueExpr> target_;
  std::unique_ptr<Expr> value_;

  DEFINE_PRINTERS(AssignExpr, "kind", kind_, "target", target_, "value",
                  value_);
};

class ExprList {
 public:
  ExprList(std::vector<Expr> exprs);

  std::vector<Expr> const& exprs() const { return exprs_; }

 private:
  std::vector<Expr> exprs_;

  DEFINE_PRINTERS(ExprList, "exprs", exprs_);
};

// An expression that can be stored to. This must either be a variable or
// an array access expression.
class LValueExpr : public util::ChoiceBase<LValueExpr,  //
                                           VarExpr, ArrayIndexExpr> {
  using ChoiceBase::ChoiceBase;
};

class Expr : public util::ChoiceBase<
                 Expr,  //
                 AddrOfExpr, SelectLitExpr, VarExpr, ArrayIndexExpr,
                 ConstValueExpr, CallExpr, ReturnExpr, BreakExpr, ContinueExpr,
                 WhileExpr, ForExpr, IfExpr, CondExpr, SwitchExpr, SwitchToExpr,
                 SendExpr, AssignExpr, IncDecExpr, ExprList> {
  using ChoiceBase::ChoiceBase;
};

// Top level declarations.

class ScriptNumDef {
 public:
  ScriptNumDef(TokenNode<int> script_num)
      : script_num_(std::move(script_num)) {}

  TokenNode<int> const& script_num() const { return script_num_; }

 private:
  TokenNode<int> script_num_;

  DEFINE_PRINTERS(ScriptNumDef, "script_num", script_num_);
};

class PublicDef {
 public:
  struct Entry {
    TokenNode<util::RefStr> name;
    TokenNode<int> index;
  };

  PublicDef(std::vector<Entry> entries) : entries_(std::move(entries)) {}

  std::vector<Entry> const& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;

  DEFINE_PRINTERS(PublicDef, "entries", entries_);
};

class ExternDef {
 public:
  struct Entry {
    TokenNode<util::RefStr> name;
    TokenNode<int> module_num;
    TokenNode<int> index;

   private:
    DEFINE_PRINTERS(Entry, "name", name, "module_num", module_num, "index",
                    index);
  };

  ExternDef(std::vector<Entry> entries) : entries_(std::move(entries)) {}

  std::vector<Entry> const& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;

  DEFINE_PRINTERS(ExternDef, "entries", entries_);
};

class GlobalDeclDef {
 public:
  struct Entry {
    VarDef name;
    TokenNode<int> index;
  };

  GlobalDeclDef(std::vector<Entry> entries) : entries_(std::move(entries)) {}

  std::vector<Entry> const& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;

  DEFINE_PRINTERS(GlobalDeclDef, "entries", entries_);
};

class ModuleVarsDef {
 public:
  enum Kind {
    GLOBAL,
    LOCAL,
  };

  struct Entry {
    VarDef name;
    TokenNode<int> index;
    std::optional<InitialValue> initial_value;
  };

  ModuleVarsDef(Kind kind, std::vector<Entry> entries)
      : kind_(kind), entries_(std::move(entries)) {}

  Kind kind() const { return kind_; }
  std::vector<Entry> const& entries() const { return entries_; }

 private:
  Kind kind_;
  std::vector<Entry> entries_;

  DEFINE_PRINTERS(ModuleVarsDef, "kind", kind_, "entries", entries_);
};

class ProcDef {
 public:
  ProcDef(TokenNode<util::RefStr> name,
          std::vector<TokenNode<util::RefStr>> args, std::vector<VarDef> locals,
          Expr body)
      : name_(std::move(name)),
        args_(std::move(args)),
        locals_(std::move(locals)),
        body_(std::move(body)) {}

  TokenNode<util::RefStr> const& name() const { return name_; }
  std::vector<TokenNode<util::RefStr>> const& args() const { return args_; }
  std::vector<VarDef> const& locals() const { return locals_; }
  Expr const& body() const { return body_; }

 private:
  TokenNode<util::RefStr> name_;
  std::vector<TokenNode<util::RefStr>> args_;
  std::vector<VarDef> locals_;
  Expr body_;

  DEFINE_PRINTERS(ProcDef, "name", name_, "args", args_, "locals", locals_,
                  "body", body_);
};

struct PropertyDef {
  TokenNode<util::RefStr> name;
  ConstValue value;

 private:
  DEFINE_PRINTERS(PropertyDef, "name", name, "value", value);
};

struct MethodNamesDecl {
  std::vector<TokenNode<util::RefStr>> names;

 private:
  DEFINE_PRINTERS(MethodNamesDecl, "names", names);
};

// A common class definition.
//
// Not to be confused with a (classdef) (which we confusingly call a
// ClassDecl) which a forward definition of class types.
class ClassDef {
 public:
  enum Kind {
    CLASS,
    OBJECT,
  };

  ClassDef(Kind kind, TokenNode<util::RefStr> name,
           std::optional<TokenNode<util::RefStr>> parent,
           std::vector<PropertyDef> properties,
           std::optional<MethodNamesDecl> method_names,
           std::vector<ProcDef> methods)
      : kind_(kind),
        name_(std::move(name)),
        parent_(std::move(parent)),
        properties_(std::move(properties)),
        method_names_(std::move(method_names)),
        methods_(std::move(methods)) {}

  Kind kind() const { return kind_; }
  TokenNode<util::RefStr> const& name() const { return name_; }
  std::optional<TokenNode<util::RefStr>> const& parent() const {
    return parent_;
  }
  std::vector<PropertyDef> const& properties() const { return properties_; }
  std::optional<MethodNamesDecl> const& method_names() const {
    return method_names_;
  }
  std::vector<ProcDef> const& methods() const { return methods_; }

 private:
  Kind kind_;
  TokenNode<util::RefStr> name_;
  std::optional<TokenNode<util::RefStr>> parent_;
  std::vector<PropertyDef> properties_;
  std::optional<MethodNamesDecl> method_names_;
  std::vector<ProcDef> methods_;

  DEFINE_PRINTERS(ClassDef, "kind", kind_, "name", name_, "parent", parent_,
                  "properties", properties_, "method_names", method_names_,
                  "methods", methods_);
};

class ClassDecl {
 public:
  ClassDecl(TokenNode<util::RefStr> name, TokenNode<int> script_num,
            TokenNode<int> class_num, std::optional<TokenNode<int>> parent_num,
            std::vector<PropertyDef> properties, MethodNamesDecl method_names)
      : name_(std::move(name)),
        script_num_(std::move(script_num)),
        class_num_(std::move(class_num)),
        parent_num_(std::move(parent_num)),
        properties_(std::move(properties)),
        method_names_(std::move(method_names)) {}

  TokenNode<util::RefStr> const& name() const { return name_; }
  TokenNode<int> const& script_num() const { return script_num_; }
  TokenNode<int> const& class_num() const { return class_num_; }
  std::optional<TokenNode<int>> const& parent_num() const {
    return parent_num_;
  }

  absl::Span<PropertyDef const> properties() const { return properties_; }
  MethodNamesDecl const& method_names() const { return method_names_; }

 private:
  TokenNode<util::RefStr> name_;
  TokenNode<int> script_num_;
  TokenNode<int> class_num_;
  std::optional<TokenNode<int>> parent_num_;
  std::vector<PropertyDef> properties_;
  MethodNamesDecl method_names_;

  DEFINE_PRINTERS(ClassDecl, "name", name_, "script_num", script_num_,
                  "class_num", class_num_, "parent_num", parent_num_,
                  "properties", properties_, "method_names", method_names_);
};

class SelectorsDecl {
 public:
  struct Entry {
    TokenNode<util::RefStr> name;
    TokenNode<int> id;
  };

  SelectorsDecl(std::vector<Entry> selectors)
      : selectors_(std::move(selectors)) {}

  std::vector<Entry> const& selectors() const { return selectors_; }

 private:
  std::vector<Entry> selectors_;

  DEFINE_PRINTERS(SelectorsDecl, "selectors", selectors_);
};

class Item : public util::ChoiceBase<Item,  //
                                     ScriptNumDef, PublicDef, ExternDef,
                                     GlobalDeclDef, ModuleVarsDef, ProcDef,
                                     ClassDef, ClassDecl, SelectorsDecl> {
  using ChoiceBase::ChoiceBase;
};

}  // namespace parsers::sci

#endif