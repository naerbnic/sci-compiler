#include "scic/sem/exprs/expr_context.hpp"

#include <map>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "scic/sem/common.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/sem/property_list.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"

namespace sem {
namespace {
template <class C, class T>
auto GetOrNull(C& c, T&& key) {
  auto it = c.find(std::forward<T>(key));
  if (it != c.end()) {
    static_assert(std::is_reference_v<decltype(*it)>,
                  "GetOrNull can only be used on collections that return a "
                  "reference.");
    return &*it;
  } else {
    return nullptr;
  }
}
class ExprEnvironmentImpl : public ExprEnvironment {
 public:
  using LocalVar = util::Choice<ParamSym, TempSym>;
  ExprEnvironmentImpl(ModuleEnvironment const* mod_env,
                      PropertyList const* prop_list,
                      std::optional<SuperInfo> super_info,
                      std::map<util::RefStr, LocalVar> local_table)
      : mod_env_(mod_env),
        prop_list_(prop_list),
        super_info_(std::move(super_info)),
        local_table_(std::move(local_table)) {}

  std::optional<SuperInfo> GetSuperInfo() const override { return super_info_; }

  std::optional<SelectorNum> LookupSelector(
      std::string_view name) const override {
    auto const* sel =
        mod_env_->global_env()->selector_table()->LookupByName(name);
    if (!sel) {
      return std::nullopt;
    }

    return sel->selector_num();
  }

  std::optional<Sym> LookupSym(std::string_view name) const override {
    // Lookup in all sources, to detect any ambiguities.
    auto const* prop = prop_list_ ? prop_list_->LookupByName(name) : nullptr;
    auto const* local = GetOrNull(local_table_, name);
    // FIXME: We need a global table.
  }

  Proc const* LookupProc(std::string_view name) const override = 0;

 private:
  ModuleEnvironment const* mod_env_;
  absl::Nullable<PropertyList const*> prop_list_;
  std::optional<SuperInfo> super_info_;
  std::map<util::RefStr, LocalVar> local_table_;
};
}  // namespace
}  // namespace sem