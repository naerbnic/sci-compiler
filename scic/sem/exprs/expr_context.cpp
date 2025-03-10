#include "scic/sem/exprs/expr_context.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "scic/sem/common.hpp"
#include "scic/sem/module_env.hpp"
#include "scic/sem/property_list.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"

namespace sem {
namespace {

template <class C, class T>
auto GetOrNull(C& c, T&& key) -> std::decay_t<C>::mapped_type const* {
  auto it = c.find(std::forward<T>(key));
  if (it != c.end()) {
    static_assert(std::is_reference_v<decltype(*it)>,
                  "GetOrNull can only be used on collections that return a "
                  "reference.");
    return &it->second;
  } else {
    return nullptr;
  }
}

template <class... Args>
bool AreAllNull(Args&&... args) {
  return ((args == nullptr) && ...);
}

class ExprEnvironmentImpl : public ExprEnvironment {
 public:
  using LocalVar = util::Choice<ParamSym, TempSym>;
  ExprEnvironmentImpl(
      ModuleEnvironment const* mod_env, PropertyList const* prop_list,
      std::optional<SuperInfo> super_info,
      std::map<util::RefStr, ParamSym, std::less<>> proc_local_table,
      std::map<util::RefStr, TempSym, std::less<>> proc_temp_table)
      : mod_env_(mod_env),
        prop_list_(prop_list),
        super_info_(std::move(super_info)),
        proc_local_table_(std::move(proc_local_table)),
        proc_temp_table_(std::move(proc_temp_table)) {}

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

  status::StatusOr<Sym> LookupSym(std::string_view name) const override {
    // We use the following order of resolution, where the results are not
    // ambiguous:
    //
    // - Params and Temps (no collision)
    // - Properties (if it exists)
    // - Globals and ModuleVars (if this is script0, check that indexes match).
    {
      auto const* param = GetOrNull(proc_local_table_, name);
      auto const* temp = GetOrNull(proc_temp_table_, name);

      if (param || temp) {
        if (param && temp) {
          return status::InvalidArgumentError("Ambiguous symbol");
        }

        if (param) {
          return *param;
        } else {
          return *temp;
        }
      }
    }

    {
      auto const* prop = prop_list_ ? prop_list_->LookupByName(name) : nullptr;

      if (prop) {
        return PropSym{
            .prop_offset = prop->index().value(),
            .selector = prop->selector(),
        };
      }
    }

    {
      auto const* global =
          mod_env_->global_env()->global_table()->LookupByName(name);
      auto const* module_var = mod_env_->local_table()->LookupByName(name);
      auto const* obj = mod_env_->object_table()->LookupByName(name);
      auto const* cls =
          mod_env_->global_env()->class_table()->LookupByName(name);

      if (module_var || global || obj || cls) {
        if (module_var && global && !obj && !cls) {
          if (mod_env_->script_num().value() == 0 &&
              module_var->index().value() == global->index().value()) {
            return LocalSym{
                .local_offset = module_var->index().value(),
            };
          } else {
            return status::InvalidArgumentError("Ambiguous symbol");
          }
        }

        std::size_t num_resolved = 0;
        if (module_var) {
          ++num_resolved;
        }
        if (global) {
          ++num_resolved;
        }
        if (obj) {
          ++num_resolved;
        }
        if (cls) {
          ++num_resolved;
        }

        if (num_resolved > 1) {
          return status::InvalidArgumentError("Ambiguous symbol");
        }

        if (module_var) {
          return LocalSym{
              .local_offset = module_var->index().value(),
          };
        } else if (global) {
          return GlobalSym{
              .global_offset = global->index().value(),
          };
        } else if (obj) {
          return ObjectSym{
              .obj = obj,
          };
        } else if (cls) {
          return ClassSym{
              .cls = cls,
          };
        } else {
          throw std::logic_error("Unreachable");
        }
      }
    }

    return status::NotFoundError(absl::StrFormat("Symbol not found: %s", name));
  }

  status::StatusOr<Proc> LookupProc(std::string_view name) const override {
    auto const* proc = mod_env_->proc_table()->LookupByName(name);
    auto const* ext =
        mod_env_->global_env()->extern_table()->LookupByName(name);

    if (proc && ext) {
      return status::InvalidArgumentError(
          absl::StrFormat("Ambiguous procedure: %s", name));
    }

    if (!proc && !ext) {
      return status::NotFoundError(
          absl::StrFormat("Procedure not found: %s", name));
    }

    if (proc) {
      return LocalProc{
          .name = proc->token_name(),
          .proc_ref = proc->ptr_ref(),
      };
    } else if (ext) {
      if (ext->script_num()) {
        return ExternProc{
            .name = ext->token_name(),
            .script_num = *ext->script_num(),
            .extern_offset = ext->index().value(),
        };
      } else {
        return KernelProc{
            .name = ext->token_name(),
            .kernel_offset = ext->index().value(),
        };
      }
    } else {
      throw std::logic_error("Unreachable");
    }
  }

 private:
  ModuleEnvironment const* mod_env_;
  absl::Nullable<PropertyList const*> prop_list_;
  std::optional<SuperInfo> super_info_;
  std::map<util::RefStr, ParamSym, std::less<>> proc_local_table_;
  std::map<util::RefStr, TempSym, std::less<>> proc_temp_table_;
};
}  // namespace

std::unique_ptr<ExprEnvironment> ExprEnvironment::Create(
    ModuleEnvironment const* mod_env, PropertyList const* prop_list,
    std::optional<SuperInfo> super_info,
    std::map<util::RefStr, ParamSym, std::less<>> proc_local_table,
    std::map<util::RefStr, TempSym, std::less<>> proc_temp_table) {
  return std::make_unique<ExprEnvironmentImpl>(
      mod_env, prop_list, std::move(super_info), std::move(proc_local_table),
      std::move(proc_temp_table));
}
}  // namespace sem