#ifndef SEM_CODE_BUILDER_HPP
#define SEM_CODE_BUILDER_HPP

#include "absl/status/status.h"
#include "scic/sem/module_env.hpp"

namespace sem {

absl::Status BuildCode(ModuleEnvironment* module_env);

}
#endif