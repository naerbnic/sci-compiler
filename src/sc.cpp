//	sc.cpp		sc
// 	the Script language compiler

#include "sc.hpp"

#include <stdlib.h>

#include <string_view>
#include <vector>

#include "argparse/argparse.hpp"
#include "asm.hpp"
#include "banner.hpp"
#include "builtins.hpp"
#include "compile.hpp"
#include "debug.hpp"
#include "define.hpp"
#include "error.hpp"
#include "input.hpp"
#include "jeff.hpp"
#include "listing.hpp"
#include "object.hpp"
#include "opcodes.hpp"
#include "output.hpp"
#include "parse.hpp"
#include "resource.hpp"
#include "share.hpp"
#include "sol.hpp"
#include "string.hpp"
#include "symtbl.hpp"
#include "text.hpp"
#include "token.hpp"
#include "update.hpp"

bool includeDebugInfo;
Compiler *sc;
int script;
bool verbose;
SciTargetArch targetArch = SciTargetArch::SCI_2;

static int totalErrors;

static void CompileFile(strptr);
static void ShowInfo();
static void InstallCommandLineDefine(std::string_view);
static void SetTargetArchitecture(std::string_view);

//	used by getargs
static std::string outDirPtr;

#if !defined(WINDOWS)

Compiler::Compiler() {
  hunkList = new CodeList;
  heapList = new FixupList;
}

Compiler::~Compiler() {
  delete hunkList;
  delete heapList;
}

static void deleteCompiler() { delete sc; }

int main(int argc, char **argv) {
  argparse::ArgumentParser parser("sc");
  parser.add_argument("-a")
      .help("abort compile if database locked")
      .default_value(false);
  parser.add_argument("-d").help("include debug info").default_value(false);
  parser.add_argument("-D")
      .help("command line define (e.g. -DMAC or -DMAC=1)")
      .action(InstallCommandLineDefine);
  parser.add_argument("-g")
      .help("maximum number of global or local variables")
      .default_value(750);
  parser.add_argument("-l")
      .help("generate a code listing")
      .default_value(false);
  parser.add_argument("-n")
      .help("no auto-naming of objects")
      .default_value(false);
  parser.add_argument("-o").help("set output directory").default_value("");
  parser.add_argument("-O")
      .help("output the 'offsets' file")
      .default_value(false);
  parser.add_argument("-s")
      .help("show forward-referenced selectors")
      .default_value(false);
  parser.add_argument("-u")
      .help("don't lock class database")
      .default_value(false);
  parser.add_argument("-v").help("verbose output").default_value(false);
  parser.add_argument("-w")
      .help("output words high-byte first (for M68000)")
      .default_value(false);
  parser.add_argument("-z").help("turn off optimization").default_value(false);
  parser.add_argument("-t")
      .help("Set the target architecture. Valid values are: SCI_1_1, SCI_2")
      .default_value("SCI_2")
      .choices("SCI_1_1", "SCI_2")
      .action(SetTargetArchitecture);
  parser.add_argument("files")
      .default_value(std::vector<std::string>())
      .remaining();
  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }
  abortIfLocked = parser.get<bool>("-a");
  includeDebugInfo = parser.get<bool>("-d");
  maxVars = parser.get<int>("-g");
  listCode = parser.get<bool>("-l");
  noAutoName = parser.get<bool>("-n");
  outDirPtr = parser.get<std::string>("-o");
  writeOffsets = parser.get<bool>("-O");
  showSelectors = parser.get<bool>("-s");
  dontLock = parser.get<bool>("-u");
  verbose = parser.get<bool>("-v");
  highByteFirst = parser.get<bool>("-w");
  noOptimize = parser.get<bool>("-z");

  char *op;
  strptr extPtr;
  int outLen;
  char fileName[_MAX_PATH + 1];

  sc = new Compiler;
  atexit(deleteCompiler);

  output("%s", banner);

#if 0
	if (writeMemSizes)
		atexit(WriteMemSizes);
#endif

  auto files = parser.get<std::vector<std::string>>("files");
  if (files.empty()) {
    std::cerr << "No input files specified" << std::endl;
    std::cerr << parser;
    return 1;
  }

  // See if the first file to be compiled exists.  If not, exit.
  auto *first_file = files[0].c_str();
  extPtr = _ExtPtr(first_file);
  MakeName(fileName, first_file, first_file, (*extPtr) ? extPtr : ".sc");
  if (access(fileName, 0) == -1) Panic("Can't find %s", fileName);

  // Make sure that any output directory is terminated with a '/'.
  if (!outDirPtr.empty()) {
    strcpy(outDir, outDirPtr.c_str());
    outLen = strlen(outDir);
    op = &outDir[outLen - 1]; /* point to last char of outDir */
    if (*op != '\\' && *op != '/' && *op != ':') {
      *++op = '/';
      *++op = '\0';
    }
  }

  // Set the include path.
  SetIncludePath();

  // Install the built-in symbols then read in and install
  // the definitions.  Lock the class database so that we're the only
  // one updating it.
  InstallBuiltIns();
  InstallObjects();
  Lock();
  errors = warnings = 0;
  theFile = OpenFileAsInput("selector", True);
  Parse();
  if (access("classdef", 0) == 0) {
    theFile = OpenFileAsInput("classdef", True);
    Parse();
  }

#if defined(PLAYGRAMMER)
  ReadDebugFile();
#endif

  theFile = OpenFileAsInput("system.sh", True);
  Parse();

  theFile = OpenFileAsInput("game.sh", False);
  if (theFile) Parse();

  totalErrors += errors;

  // Compile the files.
  for (auto const &src_file : files) {
    CompileFile(src_file.c_str());
  }

  // Write out the class table and unlock the class database.
  WriteClassTbl();
  if (writeOffsets) WritePropOffsets();
  Unlock();

  return totalErrors != 0;
}

#endif

static void CompileFile(strptr fileName) {
  char sourceFileName[_MAX_PATH + 1];
  const char *extPtr;

  // Do some initialization.
  script = -1;
  errors = warnings = 0;
  InitPublics();
  text.init();

  // Delete any free symbol tables.
  syms.delFreeTbls();

  // Look up the symbol for 'name', as it will be used in object.c.
  nameSymbol = syms.selectorSymTbl->lookup("name");

  // Open the source file.
  extPtr = _ExtPtr(fileName);
  MakeName(sourceFileName, fileName, fileName, *extPtr ? extPtr : ".sc");
  strlwr(sourceFileName);

  output("%s\n", sourceFileName);
  theFile = OpenFileAsInput(sourceFileName, True);
  curSourceFile = theFile;

  // Parse the file (don't lock the symbol tables), then assemble it.
  syms.moduleSymTbl = syms.add(ST_MEDIUM);
  Parse();
  MakeText();  // Add text to the assembly code
  if (script == -1)
    Error("No script number specified.  Can't write output files.");
  else {
    OpenListFile(sourceFileName);
    Assemble();
  }
  totalErrors += errors;

  // Write out class/selector information only if there have been no
  // errors to this point.
  if (!totalErrors) UpdateDataBase();

  // Show information.
  ShowInfo();
  if (listCode)
    CloseListFile();
  else
    DeleteListFile();

  // Delete any free symbol tables.
  syms.delFreeTbls();

  // This was used to handle custom memory management. Disabled.
#if 0  // !defined(WINDOWS) && !defined(__386__)
  if (writeMemSizes) MemDisplay();
  HEAPCHK
#endif
}

static void ShowInfo() {
  if (errors)
    output("\t%d error%s.\n", errors, errors == 1 ? "" : "s");
  else
    output("\tNo errors.\n");
}

static void InstallCommandLineDefine(std::string_view str) {
  if (str.empty()) {
    Panic("-D flag used without symbol to define");
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

  if (syms.lookup(token)) Panic("'%s' has already been defined", token);

  Symbol *sym = syms.installGlobal(token, S_DEFINE);
  sym->setStr(newStr(value));
}

static void SetTargetArchitecture(std::string_view str) {
  if (str == "SCI_1_1") {
    targetArch = SciTargetArch::SCI_1_1;
  } else if (str == "SCI_2") {
    targetArch = SciTargetArch::SCI_2;
  } else {
    Panic("Invalid target architecture: %s", str);
  }
}
