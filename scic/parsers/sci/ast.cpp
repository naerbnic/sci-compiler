#include "scic/parsers/sci/ast.hpp"

#include <optional>
#include <utility>
#include <vector>
namespace parsers::sci {

CallArgs::CallArgs(std::vector<Expr> args, std::optional<Rest> rest)
    : args_(std::move(args)), rest_(std::move(rest)) {}
CallArgs::~CallArgs() = default;

CallArgs::CallArgs(CallArgs&&) = default;
CallArgs& CallArgs::operator=(CallArgs&&) = default;

ExprList::ExprList(std::vector<Expr> exprs) : exprs_(std::move(exprs)) {}

}  // namespace parsers::sci