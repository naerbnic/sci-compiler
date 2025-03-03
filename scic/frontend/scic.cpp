#include <exception>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "scic/frontend/flags.hpp"

namespace frontend {}  // namespace frontend

int main(int argc, char** argv) {
  // Initialize Abseil symbolizer, so errors will report correct info.
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  frontend::CompilerFlags flags;
  try {
    flags = frontend::ExtractFlags(argc, argv);
  } catch (const std::exception& err) {
    return 1;
  }
  return 0;
}