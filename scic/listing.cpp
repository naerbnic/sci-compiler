//	listing.cpp		sc
// 	handle code listing

#include "scic/listing.hpp"

#include <cstdio>
#include <string_view>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/str_cat.h"
#include "scic/config.hpp"
#include "scic/error.hpp"
#include "scic/opcodes.hpp"
#include "scic/sc.hpp"
#include "scic/sol.hpp"

#define JUST_OP 0  // Only operator -- no arguments
#define OP_ARGS 1  // Operator takes arguments
#define OP_SIZE 2  // Operator takes a size

struct OpStr {
  std::string_view str;
  uint16_t info;
} static theOpCodes[] = {
    {"bnot", JUST_OP},
    {"add", JUST_OP},
    {"sub", JUST_OP},
    {"mul", JUST_OP},
    {"div", JUST_OP},
    {"mod", JUST_OP},
    {"shr", JUST_OP},
    {"shl", JUST_OP},
    {"xor", JUST_OP},
    {"and", JUST_OP},
    {"or", JUST_OP},

    {"neg", JUST_OP},
    {"not", JUST_OP},

    {"eq?", JUST_OP},
    {"ne?", JUST_OP},
    {"gt?", JUST_OP},
    {"ge?", JUST_OP},
    {"lt?", JUST_OP},
    {"le?", JUST_OP},

    {"ugt?", JUST_OP},
    {"uge?", JUST_OP},
    {"ult?", JUST_OP},
    {"ule?", JUST_OP},

    {"bt", OP_ARGS | OP_SIZE},
    {"bnt", OP_ARGS | OP_SIZE},
    {"jmp", OP_ARGS | OP_SIZE},

    {"ldi", OP_ARGS | OP_SIZE},
    {"push", JUST_OP},
    {"pushi", OP_ARGS | OP_SIZE},
    {"toss", JUST_OP},
    {"dup", JUST_OP},
    {"link", OP_ARGS | OP_SIZE},

    {"call", OP_ARGS | OP_SIZE},
    {"callk", OP_ARGS | OP_SIZE},
    {"callb", OP_ARGS | OP_SIZE},
    {"calle", OP_ARGS | OP_SIZE},

    {"ret", JUST_OP},

    {"send", JUST_OP},
    {"DUMMY", JUST_OP},
    {"DUMMY", JUST_OP},

    {"class", OP_ARGS | OP_SIZE},
    {"DUMMY", JUST_OP},
    {"self", JUST_OP},
    {"super", OP_ARGS | OP_SIZE},
    {"&rest", OP_ARGS | OP_SIZE},

    {"lea", OP_ARGS | OP_SIZE},
    {"selfID", JUST_OP},
    {"DUMMY", JUST_OP},
    {"pprev", JUST_OP},

    {"pToa", OP_ARGS | OP_SIZE},
    {"aTop", OP_ARGS | OP_SIZE},
    {"pTos", OP_ARGS | OP_SIZE},
    {"sTop", OP_ARGS | OP_SIZE},
    {"ipToa", OP_ARGS | OP_SIZE},
    {"dpToa", OP_ARGS | OP_SIZE},
    {"ipTos", OP_ARGS | OP_SIZE},
    {"dpTos", OP_ARGS | OP_SIZE},

    {"lofsa", OP_ARGS | OP_SIZE},
    {"lofss", OP_ARGS | OP_SIZE},
    {"push0", JUST_OP},
    {"push1", JUST_OP},
    {"push2", JUST_OP},
    {"pushSelf", JUST_OP},
};

namespace {
class ListingFileImpl : public ListingFile {
 public:
  explicit ListingFileImpl(absl::Nonnull<FILE*> listFile,
                           absl::Nullable<FILE*> sourceFile)
      : sourceLineNum_(0), listFile_(listFile), sourceFile_(sourceFile) {}

  ~ListingFileImpl() override {
    fclose(listFile_);
    if (sourceFile_) {
      fclose(sourceFile_);
    };
  }

  void ListOp(std::size_t offset, uint8_t theOp) override {
    OpStr* oPtr;
    std::string_view op;
    std::string scratch;
    bool hasArgs;
    bool addSize;

    ListOffset(offset);

    if (!(theOp & OP_LDST)) {
      oPtr = &theOpCodes[(theOp & ~OP_BYTE) / 2];
      hasArgs = oPtr->info & OP_ARGS;
      addSize = oPtr->info & OP_SIZE;
      if (addSize) {
        scratch = std::string(oPtr->str);
        absl::StrAppend(&scratch, (theOp & OP_BYTE) ? ".b" : ".w");
        op = scratch;
      } else {
        op = oPtr->str;
      }

    } else {
      switch (theOp & OP_TYPE) {
        case OP_LOAD:
          scratch.push_back('l');
          break;

        case OP_STORE:
          scratch.push_back('s');
          break;

        case OP_INC:
          scratch.push_back('+');
          break;

        case OP_DEC:
          scratch.push_back('-');
          break;
      }

      if (theOp & OP_STACK)
        scratch.push_back('s');
      else
        scratch.push_back('a');

      switch (theOp & OP_VAR) {
        case OP_GLOBAL:
          scratch.push_back('g');
          break;

        case OP_LOCAL:
          scratch.push_back('l');
          break;

        case OP_TMP:
          scratch.push_back('t');
          break;

        case OP_PARM:
          scratch.push_back('p');
          break;
      }

      if (theOp & OP_INDEX) scratch.push_back('i');

      op = scratch;
      addSize = hasArgs = true;
    }

    if (hasArgs)
      ListingNoCRLF("%-5s", op);
    else
      Listing("%s", op);
  }

  void ListWord(std::size_t offset, uint16_t w) override {
    ListAsCode(offset, "word\t$%x", (SCIUWord)w);
  }

  void ListByte(std::size_t offset, uint8_t b) override {
    ListAsCode(offset, "byte\t$%x", b);
  }

  void ListOffset(std::size_t offset) override {
    ListingNoCRLF("\t\t%5x\t", offset);
  }

  void ListText(std::size_t offset, std::string_view s) override {
    std::string line;

    ListAsCode(offset, "text");

    line.push_back('"');  // start with a quote
    auto curr_it = s.begin();
    while (1) {
      // Copy from the text until the output line is full.

      while (line.length() <= 80 && curr_it != s.end() && *curr_it != '\n') {
        if (*curr_it == '%') line.push_back('%');
        line.push_back(*curr_it++);
      }

      // If the line is not full, we are done.  Finish with a quote.

      if (line.length() <= 80) {
        line.append("\"\n");
        Listing("%s", line);
        break;
      }

      // Scan back in the text to a word break, then print the line.
      // FIXME: What happens if there is no break?
      std::string_view line_view = line;
      auto last_space_index = line_view.rfind(' ');
      if (last_space_index == std::string::npos) {
        line_view = line_view.substr(0, 80);
        curr_it -= line.length() - 80;
      } else {
        line_view = line_view.substr(0, last_space_index);
        curr_it -= line.length() - last_space_index;
        ++curr_it;  // point past the space
      }

      Listing("%s", line_view);

      line.clear();
    }
  }

  void ListSourceLine(int num) override {
    if (!sourceFile_) return;
    char buf[512];
    for (; sourceLineNum_ < num; sourceLineNum_++) {
      if (!fgets(buf, sizeof buf, sourceFile_)) {
        Panic("Can't read source line %d", sourceLineNum_);
      }
    }
    ListingNoCRLF("%s", buf);
  }

 protected:
  void ListingImpl(absl::FunctionRef<bool(FILE*)> print_func) override {
    if (!print_func(listFile_) || putc('\n', listFile_) == EOF) {
      throw std::runtime_error("Error writing list file");
    }
  }
  void ListAsCodeImpl(std::size_t offset,
                      absl::FunctionRef<bool(FILE*)> print_func) override {
    ListOffset(offset);

    ListingImpl(print_func);
  }
  void ListingNoCRLFImpl(absl::FunctionRef<bool(FILE*)> print_func) override {
    if (!print_func(listFile_)) {
      throw std::runtime_error("Error writing list file");
    }
  }

 private:
  int sourceLineNum_;
  absl::Nonnull<FILE*> listFile_;
  absl::Nullable<FILE*> sourceFile_;
};

class NullListingFileImpl : public ListingFile {
 public:
  NullListingFileImpl() = default;
  void ListOp(std::size_t offset, uint8_t theOp) override {}

  void ListWord(std::size_t offset, uint16_t) override {}

  void ListByte(std::size_t offset, uint8_t) override {}

  void ListOffset(std::size_t offset) override {}

  void ListText(std::size_t offset, std::string_view s) override {}

  void ListSourceLine(int num) override {}

 protected:
  void ListingImpl(absl::FunctionRef<bool(FILE*)> print_func) override {}
  void ListAsCodeImpl(std::size_t offset,
                      absl::FunctionRef<bool(FILE*)> print_func) override {}
  void ListingNoCRLFImpl(absl::FunctionRef<bool(FILE*)> print_func) override {}
};
}  // namespace

std::unique_ptr<ListingFile> ListingFile::Open(
    std::string_view sourceFileName) {
  auto listName = gConfig->outDir / absl::StrFormat("%d.sl", gScript);
  FILE* listFile = nullptr;
  FILE* sourceFile = nullptr;

  if (!(listFile = fopen(listName.string().c_str(), "wt"))) {
    Panic("Can't open %s for listing", listName.string());
  }

  if (gConfig->includeDebugInfo) {
    if (!(sourceFile = fopen(std::string(sourceFileName).c_str(), "rt")))
      Panic("Can't open %s for source lines in listing", sourceFileName);
  }

  auto result = std::make_unique<ListingFileImpl>(listFile, sourceFile);

  result->Listing("\n\t\t\t\tListing of %s:\t[script %d]\n\n", sourceFileName,
                  (SCIUWord)gScript);
  result->Listing("LINE/\tOFFSET\tCODE\t\t\t\tNAME");
  result->Listing("LABEL\t(HEX)\n");

  return result;
}

std::unique_ptr<ListingFile> ListingFile::Null() {
  return std::make_unique<NullListingFileImpl>();
}
