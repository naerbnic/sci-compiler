#ifndef SEM_CODE_BUILDER_HPP
#define SEM_CODE_BUILDER_HPP

#include "scic/sem/module_env.hpp"
#include "scic/status/status.hpp"

namespace sem {

status::Status BuildCode(ModuleEnvironment const* module_env);

}
#endif