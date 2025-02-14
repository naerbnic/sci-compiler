#ifndef SCIC_PARSERS_SCI_AST_HPP
#define SCIC_PARSERS_SCI_AST_HPP

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "scic/text/text_range.hpp"
#include "util/choice.hpp"

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
  TokenNode(T value, text::TextRange text_range)
      : value_(std::move(value)), text_range_(std::move(text_range)) {}

  T const& value() const { return value_; }
  text::TextRange const& text_range() const { return text_range_; }

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
  text::TextRange text_range_;
};

// AST Nodes for the SCI language parse tree.

// Variable Definitions. This can be either a single variable, or
// an array definition.

class SingleVarDef {
 public:
  SingleVarDef(TokenNode<std::string> name) : name_(std::move(name)) {}

  TokenNode<std::string> const& name() const { return name_; }

 private:
  TokenNode<std::string> name_;
};

class ArrayVarDef {
 public:
  ArrayVarDef(TokenNode<std::string> name, TokenNode<int> size)
      : name_(std::move(name)), size_(std::move(size)) {}

  TokenNode<std::string> const& name() const { return name_; }
  TokenNode<int> const& size() const { return size_; }

 private:
  TokenNode<std::string> name_;
  TokenNode<int> size_;
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
};

class StringConstValue {
 public:
  StringConstValue(TokenNode<std::string> value) : value_(std::move(value)) {}

  TokenNode<std::string> const& value() const { return value_; }

 private:
  TokenNode<std::string> value_;
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
};

class InitialValue
    : public util::ChoiceBase<InitialValue, ConstValue, ArrayInitialValue> {
  using ChoiceBase::ChoiceBase;
};

// Expr Definitions

class Expr;

class CallArgs {
 public:
  struct Rest {
    std::optional<TokenNode<std::string>> rest_var;
  };

  explicit CallArgs(std::vector<std::unique_ptr<Expr>> args,
                    std::optional<Rest>)
      : args_(std::move(args)) {}

  std::vector<std::unique_ptr<Expr>> const& args() const { return args_; }
  std::optional<Rest> const& rest() const { return rest_; }

 private:
  std::vector<std::unique_ptr<Expr>> args_;
  std::optional<Rest> rest_;
};

class AddrOfExpr {
 public:
  AddrOfExpr(std::unique_ptr<Expr> expr) : expr_(std::move(expr)) {}

  Expr const& expr() const { return *expr_; }

 private:
  std::unique_ptr<Expr> expr_;
};

class SelectLitExpr {
 public:
  SelectLitExpr(TokenNode<std::string> selector)
      : selector_(std::move(selector)) {}

  TokenNode<std::string> const& selector() const { return selector_; }

 private:
  TokenNode<std::string> selector_;
};

// A plain variable reference.
class VarExpr {
 public:
  VarExpr(TokenNode<std::string> name) : name_(std::move(name)) {}

  TokenNode<std::string> const& name() const { return name_; }

 private:
  TokenNode<std::string> name_;
};

class ArrayIndexExpr {
 public:
  ArrayIndexExpr(TokenNode<std::string> var_name, std::unique_ptr<Expr> index)
      : var_name_(std::move(var_name)), index_(std::move(index)) {}

  TokenNode<std::string> const& var_name() const { return var_name_; }
  Expr const& index() const { return *index_; }

 private:
  TokenNode<std::string> var_name_;
  std::unique_ptr<Expr> index_;
};

class ConstValueExpr {
 public:
  ConstValueExpr(ConstValue value) : value_(std::move(value)) {}

  ConstValue const& value() const { return value_; }

 private:
  ConstValue value_;
};

class RestExpr {
 public:
  RestExpr(text::TextRange source) : source_(std::move(source)) {}

  text::TextRange const& source() const { return source_; }

 private:
  text::TextRange source_;
};

// A call expresion, of (<name> <arg> ...). Aside from control flow
// structures, and send expressions, other expressions of that form
// are represented as calls.
class CallExpr {
 public:
  struct Rest {
    std::optional<TokenNode<std::string>> rest_var;
  };

  CallExpr(TokenNode<std::string> name, CallArgs call_args)
      : name_(std::move(name)), call_args_(std::move(call_args)) {}

  TokenNode<std::string> const& name() const { return name_; }
  CallArgs const& call_args() const { return call_args_; }

 private:
  TokenNode<std::string> name_;
  CallArgs call_args_;
  std::optional<Rest> rest_;
};

class ReturnExpr {
 public:
  ReturnExpr(std::optional<std::unique_ptr<Expr>> expr)
      : expr_(std::move(expr)) {}

  std::optional<std::unique_ptr<Expr>> const& expr() const { return expr_; }

 private:
  std::optional<std::unique_ptr<Expr>> expr_;
};

class BreakExpr {
 public:
  BreakExpr(std::optional<TokenNode<int>> level) : level_(std::move(level)) {}

  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<TokenNode<int>> level_;
};

class BreakIfExpr {
 public:
  BreakIfExpr(std::optional<std::unique_ptr<Expr>> condition,
              std::optional<TokenNode<int>> level)
      : condition_(std::move(condition)), level_(std::move(level)) {}

  std::optional<std::unique_ptr<Expr>> const& condition() const {
    return condition_;
  }
  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<std::unique_ptr<Expr>> condition_;
  std::optional<TokenNode<int>> level_;
};

class ContinueExpr {
 public:
  ContinueExpr(std::optional<TokenNode<int>> level)
      : level_(std::move(level)) {}

  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<TokenNode<int>> level_;
};

class ContIfExpr {
 public:
  ContIfExpr(std::optional<std::unique_ptr<Expr>> condition,
             std::optional<TokenNode<int>> level)
      : condition_(std::move(condition)), level_(std::move(level)) {}

  std::optional<std::unique_ptr<Expr>> const& condition() const {
    return condition_;
  }
  std::optional<TokenNode<int>> const& level() const { return level_; }

 private:
  std::optional<std::unique_ptr<Expr>> condition_;
  std::optional<TokenNode<int>> level_;
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
};

class SwitchExpr {
 public:
  struct Case {
    std::unique_ptr<ConstValue> value;
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
  std::vector<std::unique_ptr<Expr>> const& cases() const { return cases_; }
  std::optional<std::unique_ptr<Expr>> const& else_case() const {
    return else_case_;
  }

 private:
  std::unique_ptr<Expr> switch_expr_;
  std::vector<std::unique_ptr<Expr>> cases_;
  std::optional<std::unique_ptr<Expr>> else_case_;
};

class IncDecExpr {
 public:
  enum Kind {
    INC,
    DEC,
  };

  IncDecExpr(Kind kind, TokenNode<std::string> var)
      : kind_(kind), var_(std::move(var)) {}

  Kind kind() const { return kind_; }
  TokenNode<std::string> const& var() const { return var_; }

 private:
  Kind kind_;
  TokenNode<std::string> var_;
};

class SelfSendTarget {};
class SuperSendTarget {};
class ExprSendTarget {
 public:
  explicit ExprSendTarget(std::unique_ptr<Expr> target)
      : target_(std::move(target)) {}

  Expr const& target() const { return *target_; }

 private:
  std::unique_ptr<Expr> target_;
};

class SendTarget
    : public util::ChoiceBase<SendTarget,  //
                              SelfSendTarget, SuperSendTarget, ExprSendTarget> {
  using ChoiceBase::ChoiceBase;
};

class PropReadSendClause {
 public:
  explicit PropReadSendClause(TokenNode<std::string> prop_name)
      : prop_name_(std::move(prop_name)) {}

  TokenNode<std::string> const& prop_name() const { return prop_name_; }

 private:
  TokenNode<std::string> prop_name_;
};

class MethodSendClause {
 public:
  MethodSendClause(TokenNode<std::string> method_name, CallArgs call_args)
      : selector_(std::move(method_name)), call_args_(std::move(call_args)) {}

  TokenNode<std::string> const& selector() const { return selector_; }
  CallArgs const& call_args() const { return call_args_; }

 private:
  TokenNode<std::string> selector_;
  CallArgs call_args_;
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
};

class AssignExpr {
 public:
  AssignExpr(TokenNode<std::string> var, std::unique_ptr<Expr> value)
      : var_(std::move(var)), value_(std::move(value)) {}

  TokenNode<std::string> const& var() const { return var_; }
  Expr const& value() const { return *value_; }

 private:
  TokenNode<std::string> var_;
  std::unique_ptr<Expr> value_;
};

class ExprList {
 public:
  ExprList(std::vector<std::unique_ptr<Expr>> exprs)
      : exprs_(std::move(exprs)) {}

  std::vector<std::unique_ptr<Expr>> const& exprs() const { return exprs_; }

 private:
  std::vector<std::unique_ptr<Expr>> exprs_;
};

class Expr : public util::ChoiceBase<
                 Expr,  //
                 AddrOfExpr, SelectLitExpr, VarExpr, ArrayIndexExpr,
                 ConstValueExpr, RestExpr, CallExpr, ReturnExpr, BreakExpr,
                 BreakIfExpr, ContinueExpr, ContIfExpr, IfExpr, CondExpr,
                 SwitchExpr, SwitchToExpr, SendExpr, AssignExpr, ExprList> {
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
};

class PublicDef {
 public:
  struct Entry {
    TokenNode<std::string> name;
    TokenNode<int> index;
  };

  PublicDef(std::vector<Entry> entries) : entries_(std::move(entries)) {}

  std::vector<Entry> const& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;
};

class ExternDef {
 public:
  struct Entry {
    TokenNode<std::string> name;
    TokenNode<int> module_num;
    TokenNode<int> index;
  };

  ExternDef(std::vector<Entry> entries) : entries_(std::move(entries)) {}

  std::vector<Entry> const& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;
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
};

class EnumDef {
 public:
  struct Entry {
    TokenNode<std::string> name;
    TokenNode<int> value;
  };

  EnumDef(std::optional<TokenNode<int>> initial_value,
          std::vector<Entry> entries)
      : initial_value_(std::move(initial_value)),
        entries_(std::move(entries)) {}

 private:
  std::optional<TokenNode<int>> initial_value_;
  std::vector<Entry> entries_;
};

class ProcDef {
 public:
  ProcDef(TokenNode<std::string> name, std::vector<TokenNode<std::string>> args,
          std::vector<VarDef> locals, std::unique_ptr<Expr> body)
      : name_(std::move(name)),
        args_(std::move(args)),
        locals_(std::move(locals)),
        body_(std::move(body)) {}

  TokenNode<std::string> const& name() const { return name_; }
  std::vector<TokenNode<std::string>> const& args() const { return args_; }
  std::vector<VarDef> const& locals() const { return locals_; }
  Expr const& body() const { return *body_; }

 private:
  TokenNode<std::string> name_;
  std::vector<TokenNode<std::string>> args_;
  std::vector<VarDef> locals_;
  std::unique_ptr<Expr> body_;
};

struct PropertyDef {
  TokenNode<std::string> name;
  ConstValue value;
};

struct MethodNamesDecl {
  std::vector<TokenNode<std::string>> names;
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

  ClassDef(Kind kind, TokenNode<std::string> name,
           std::optional<TokenNode<std::string>> parent,
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
  TokenNode<std::string> const& name() const { return name_; }
  std::optional<TokenNode<std::string>> const& parent() const {
    return parent_;
  }
  std::vector<PropertyDef> const& properties() const { return properties_; }
  std::optional<MethodNamesDecl> const& method_names() const {
    return method_names_;
  }
  std::vector<ProcDef> const& methods() const { return methods_; }

 private:
  Kind kind_;
  TokenNode<std::string> name_;
  std::optional<TokenNode<std::string>> parent_;
  std::vector<PropertyDef> properties_;
  std::optional<MethodNamesDecl> method_names_;
  std::vector<ProcDef> methods_;
};

class ClassDecl {
 public:
  ClassDecl(TokenNode<std::string> name, TokenNode<int> script_num,
            TokenNode<int> class_num, std::optional<TokenNode<int>> parent_num,
            std::vector<PropertyDef> properties, MethodNamesDecl method_names)
      : name_(std::move(name)),
        script_num_(std::move(script_num)),
        class_num_(std::move(class_num)),
        parent_num_(std::move(parent_num)),
        properties_(std::move(properties)),
        method_names_(std::move(method_names)) {}

  TokenNode<std::string> const& name() const { return name_; }
  TokenNode<int> const& script_num() const { return script_num_; }
  TokenNode<int> const& class_num() const { return class_num_; }
  std::optional<TokenNode<int>> const& parent_num() const {
    return parent_num_;
  }

 private:
  TokenNode<std::string> name_;
  TokenNode<int> script_num_;
  TokenNode<int> class_num_;
  std::optional<TokenNode<int>> parent_num_;
  std::vector<PropertyDef> properties_;
  MethodNamesDecl method_names_;
};

class SelectorsDecl {
 public:
  struct Entry {
    TokenNode<std::string> name;
    TokenNode<int> id;
  };

  SelectorsDecl(std::vector<Entry> selectors)
      : selectors_(std::move(selectors)) {}

  std::vector<Entry> const& selectors() const { return selectors_; }

 private:
  std::vector<Entry> selectors_;
};

class Item
    : public util::ChoiceBase<Item,  //
                              ScriptNumDef, PublicDef, ExternDef, GlobalDeclDef,
                              ModuleVarsDef, EnumDef, ProcDef, ClassDef,
                              ClassDecl, SelectorsDecl> {
  using ChoiceBase::ChoiceBase;
};

}  // namespace parsers::sci

#endif