//	sc.cpp		sc
// 	the Script language compiler

#include "scic/legacy/sc.hpp"

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_format.h"
#include "argparse/argparse.hpp"
#include "scic/legacy/banner.hpp"
#include "scic/legacy/builtins.hpp"
#include "scic/codegen/text_sink.hpp"
#include "scic/legacy/config.hpp"
#include "scic/legacy/error.hpp"
#include "scic/legacy/global_compiler.hpp"
#include "scic/legacy/input.hpp"
#include "scic/legacy/object.hpp"
#include "scic/legacy/output.hpp"
#include "scic/legacy/parse.hpp"
#include "scic/legacy/parse_class.hpp"
#include "scic/legacy/parse_context.hpp"
#include "scic/legacy/share.hpp"
#include "scic/legacy/symtbl.hpp"
#include "scic/legacy/symtypes.hpp"
#include "scic/legacy/update.hpp"
#include "util/platform/platform.hpp"

int gScript;

static int totalErrors;

static void CompileFile(std::string_view, bool listCode);
static void ShowInfo();
static void InstallCommandLineDefine(std::string_view);
static SciTargetArch GetTargetArchitecture(std::string_view);

int main(int argc, char **argv) {
  // Initialize Abseil symbolizer, so errors will report correct info.
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
  argparse::ArgumentParser program("sc", gBanner);
  program.add_argument("-a")
      .help("abort compile if database locked")
      .default_value(false)
      .flag();
  program.add_argument("-d")
      .help("include debug info")
      .default_value(false)
      .flag();
  program.add_argument("-D")
      .help("command line define (e.g. -DMAC or -DMAC=1)")
      .action(InstallCommandLineDefine)
      .append()
      .nargs(1);
  program.add_argument("-g")
      .help("maximum number of global or local variables")
      .default_value(750ul);
  program.add_argument("-l")
      .help("generate a code listing")
      .default_value(false)
      .flag();
  program.add_argument("-n")
      .help("no auto-naming of objects")
      .default_value(false)
      .flag();
  program.add_argument("-o").help("set output directory").default_value("");
  program.add_argument("-O")
      .help("output the 'offsets' file")
      .default_value(false)
      .flag();
  program.add_argument("-s")
      .help("show forward-referenced selectors")
      .default_value(false)
      .flag();
  program.add_argument("-u")
      .help("don't lock class database")
      .default_value(false)
      .flag();
  program.add_argument("-v").help("verbose output").default_value(false).flag();
  program.add_argument("-w")
      .help("output words high-byte first (for M68000)")
      .default_value(false)
      .flag();
  program.add_argument("-z")
      .help("turn off optimization")
      .default_value(false)
      .flag();
  program.add_argument("-t")
      .help("Set the target architecture. Valid values are: SCI_1_1, SCI_2")
      .default_value(std::string{"SCI_2"});
  program.add_argument("--selector_file")
      .help("The selector file to use during compilation")
      .default_value(std::string{"selector"});
  program.add_argument("--classdef_file")
      .help("The class definition file to use during compilation")
      .default_value(std::string{"classdef"});
  program.add_argument("--system_header")
      .help("The system header file to use during compilation")
      .default_value(std::string{"system.sh"});
  program.add_argument("--game_header")
      .help("The game header file to use during compilation")
      .default_value(std::string{"game.sh"});
  program.add_argument("-I", "--include_path")
      .help("List of directories to use for include files")
      .default_value(std::vector<std::string>())
      .append()
      .nargs(1);
  program.add_argument("files")
      .default_value(std::vector<std::string>())
      .remaining();

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  ToolConfig config = {
      .abortIfLocked = program.get<bool>("-a"),
      .includeDebugInfo = program.get<bool>("-d"),
      .maxVars = program.get<std::size_t>("-g"),
      .noAutoName = program.get<bool>("-n"),
      .outDir = program.get<std::string>("-o"),
      .writeOffsets = program.get<bool>("-O"),
      .showSelectors = program.get<bool>("-s"),
      .dontLock = program.get<bool>("-u"),
      .verbose = program.get<bool>("-v"),
      .highByteFirst = program.get<bool>("-w"),
      .noOptimize = program.get<bool>("-z"),
      .targetArch = GetTargetArchitecture(program.get<std::string>("-t")),
  };

  gConfig = &config;

  bool listCode = program.get<bool>("-l");
  auto selector_file = program.get<std::string>("--selector_file");
  auto classdef_file = program.get<std::string>("--classdef_file");
  auto system_header = program.get<std::string>("--system_header");
  auto game_header = program.get<std::string>("--game_header");
  auto cli_include_path = program.get<std::vector<std::string>>("-I");
  auto files = program.get<std::vector<std::string>>("files");

  if (files.empty()) {
    std::cerr << "No input files specified" << std::endl;
    std::cerr << program;
    return 1;
  }

  // See if the first file to be compiled exists.  If not, exit.
  if (!FileExists(files[0]))
    throw std::runtime_error(absl::StrFormat("Can't find %s", files[0]));

  // Set the include path.
  gInputState.SetIncludePath(cli_include_path);

  // Install the built-in symbols then read in and install
  // the definitions.  Lock the class database so that we're the only
  // one updating it.
  InstallBuiltIns();
  InstallObjects();
  Lock();
  auto unlock = absl::Cleanup([] { Unlock(); });

  gNumErrors = gNumWarnings = 0;
  gInputState.OpenFileAsInput(selector_file, true);
  Parse();
  if (FileExists(classdef_file)) {
    gInputState.OpenFileAsInput(classdef_file, true);
    Parse();
  }

  gInputState.OpenFileAsInput(system_header, true);
  Parse();

  gInputState.OpenFileAsInput(game_header, false);
  if (!gInputState.IsDone()) Parse();

  totalErrors += gNumErrors;

  // Compile the files.
  for (auto const &src_file : files) {
    CompileFile(src_file, listCode);
  }

  // Write out the class table and unlock the class database.
  WriteClassTbl();
  if (gConfig->writeOffsets) WritePropOffsets();

  return totalErrors != 0;
}

static void CompileFile(std::string_view fileName, bool listCode) {
  // Do some initialization.
  gScript = -1;
  gNumErrors = gNumWarnings = 0;

  // Delete any free symbol tables.
  gSyms.delFreeTbls();

  // Look up the symbol for 'name', as it will be used in object.c.
  gNameSymbol = gSyms.selectorSymTbl->lookup("name");

  // Open the source file.

  std::string sourceFileName(fileName);

  output("%s\n", sourceFileName);
  gInputState.OpenTopLevelFile(sourceFileName, true);

  // Parse the file (don't lock the symbol tables), then assemble it.
  gSyms.moduleSymTbl = gSyms.add(ST_MEDIUM);
  Parse();
  if (gScript == -1)
    Error("No script number specified.  Can't write output files.");
  else {
    auto path = gConfig->outDir / absl::StrFormat("%d.sl", gScript);
    auto listSink = listCode ? codegen::TextSink::FileTrunc(path)
                             : codegen::TextSink::Null();
    auto obj_files = OpenObjFiles(gScript);
    gSc->Assemble(gInputState.GetTopLevelFileName(), gScript, listSink.get(),
                  &obj_files);
  }
  totalErrors += gNumErrors;

  // Write out class/selector information only if there have been no
  // errors to this point.
  if (!totalErrors) UpdateDataBase();

  // Show information.
  ShowInfo();

  // Delete any free symbol tables.
  gSyms.delFreeTbls();
  gSc = nullptr;
}

static void ShowInfo() {
  if (gNumErrors)
    output("\t%d error%s.\n", gNumErrors, gNumErrors == 1 ? "" : "s");
  else
    output("\tNo errors.\n");
}

static void InstallCommandLineDefine(std::string_view str) {
  if (str.empty()) {
    throw std::runtime_error("-D flag used without symbol to define");
  }
  std::string_view token;
  std::string_view value;
  auto eq_index = str.find('=');
  if (eq_index == std::string_view::npos) {
    token = str;
    value = "1";
  } else {
    token = str.substr(0, eq_index);
    value = str.substr(eq_index + 1);
  }

  if (gSyms.lookup(token))
    throw std::runtime_error(
        absl::StrFormat("'%s' has already been defined", token));

  Symbol *sym = gSyms.installGlobal(token, S_DEFINE);
  sym->setStr(std::string(value));
}

static SciTargetArch GetTargetArchitecture(std::string_view str) {
  if (str == "SCI_1_1") {
    return SciTargetArch::SCI_1_1;
  } else if (str == "SCI_2") {
    return SciTargetArch::SCI_2;
  } else {
    throw std::runtime_error(
        absl::StrFormat("Invalid target architecture: %s", str));
  }
}
