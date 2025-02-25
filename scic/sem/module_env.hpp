#ifndef SEM_MODULE_ENV_HPP
#define SEM_MODULE_ENV_HPP

#include <cstddef>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/class_table.hpp"
#include "scic/sem/common.hpp"
#include "scic/sem/extern_table.hpp"
#include "scic/sem/input.hpp"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "scic/sem/public_table.hpp"
#include "scic/sem/selector_table.hpp"
#include "util/types/choice.hpp"

namespace sem {

class GlobalEnvironment {
 public:
  GlobalEnvironment(std::unique_ptr<SelectorTable> selector_table,
                    std::unique_ptr<ClassTable> class_table,
                    std::unique_ptr<ExternTable> extern_table)
      : selector_table_(std::move(selector_table)),
        class_table_(std::move(class_table)),
        extern_table_(std::move(extern_table)) {}

  SelectorTable const* selector_table() const { return selector_table_.get(); }
  ClassTable const* class_table() const { return class_table_.get(); }
  ExternTable const* extern_table() const { return extern_table_.get(); }

 private:
  std::unique_ptr<SelectorTable> selector_table_;
  std::unique_ptr<ClassTable> class_table_;
  std::unique_ptr<ExternTable> extern_table_;
};

class ModuleEnvironment {
 public:
  ModuleEnvironment(GlobalEnvironment const* global_env, ScriptNum script_num,
                    std::unique_ptr<codegen::CodeGenerator> codegen,
                    std::unique_ptr<ObjectTable> object_table,
                    std::unique_ptr<ProcTable> proc_table,
                    std::unique_ptr<PublicTable> public_table)
      : global_env_(std::move(global_env)),
        script_num_(script_num),
        codegen_(std::move(codegen)),
        object_table_(std::move(object_table)),
        proc_table_(std::move(proc_table)),
        public_table_(std::move(public_table)) {}

  GlobalEnvironment const* global_env() const { return global_env_; }
  ScriptNum script_num() const { return script_num_; }
  codegen::CodeGenerator* codegen() const { return codegen_.get(); }
  ObjectTable const* object_table() const { return object_table_.get(); }
  ProcTable const* proc_table() const { return proc_table_.get(); }
  PublicTable const* public_table() const { return public_table_.get(); }

 private:
  GlobalEnvironment const* global_env_;
  ScriptNum script_num_;
  std::unique_ptr<codegen::CodeGenerator> codegen_;
  std::unique_ptr<ObjectTable> object_table_;
  std::unique_ptr<ProcTable> proc_table_;
  std::unique_ptr<PublicTable> public_table_;
};

class CompilationEnvironment {
 public:
  CompilationEnvironment(
      std::unique_ptr<GlobalEnvironment> global_env,
      std::map<ScriptNum, std::unique_ptr<ModuleEnvironment>> module_envs)
      : global_env_(std::move(global_env)),
        module_envs_(std::move(module_envs)) {}

  GlobalEnvironment const* global_env() const { return global_env_.get(); }

  std::vector<ModuleEnvironment const*> module_envs() const {
    std::vector<ModuleEnvironment const*> result;
    for (auto& pair : module_envs_) {
      result.push_back(pair.second.get());
    }
    return result;
  }

  ModuleEnvironment const* FindModuleEnvironmentByScriptNum(
      ScriptNum script_num) const {
    return module_envs_.at(script_num).get();
  }

 private:
  std::unique_ptr<GlobalEnvironment> global_env_;
  std::map<ScriptNum, std::unique_ptr<ModuleEnvironment>> module_envs_;
};

// The environment of code compilation within a specific procedure.
class ProcedureEnvironment {
 public:
  struct MethodName {
    util::Choice<Class const*, Object const*> parent;
    NameToken meth_name;
  };

  struct FreeProcName {
    NameToken name;
  };

  using ProcName = util::Choice<MethodName, FreeProcName>;

  ProcedureEnvironment(ModuleEnvironment const* module_env,
                       std::size_t num_params, std::size_t num_temps,
                       ProcName method_name)
      : module_env_(module_env),
        num_params_(num_params),
        num_temps_(num_temps),
        proc_context_(std::move(method_name)) {}

  ModuleEnvironment const* module_env() const { return module_env_; }
  std::size_t num_params() const { return num_params_; }
  std::size_t num_temps() const { return num_temps_; }

 private:
  // The containing module environment.
  ModuleEnvironment const* module_env_;

  // The number of params and temporaries in the procedure.
  std::size_t num_params_;
  std::size_t num_temps_;

  ProcName proc_context_;
};

absl::StatusOr<CompilationEnvironment> BuildCompilationEnvironment(
    codegen::CodeGenerator::Options codegen_options, Input const& input);

}  // namespace sem

#endif