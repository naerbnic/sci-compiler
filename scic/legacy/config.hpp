// config.hpp
//
// Configuration data structures for the entire program.

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <filesystem>

// The target SCI architecture for the scripts.
enum class SciTargetArch {
  SCI_1_1,
  SCI_2,
};

struct ToolConfig {
  bool abortIfLocked;
  bool includeDebugInfo;
  std::size_t maxVars;
  bool noAutoName;
  std::filesystem::path outDir;
  bool writeOffsets;
  bool showSelectors;
  bool dontLock;
  bool verbose;
  bool highByteFirst;
  bool noOptimize;
  SciTargetArch targetArch;
};

extern ToolConfig const* gConfig;

#endif