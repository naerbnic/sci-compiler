//	listing.cpp		sc
// 	handle code listing

#include "scic/codegen/listing.hpp"

#include <sys/types.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/function_ref.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "scic/opcodes.hpp"

namespace codegen {
namespace {

enum OpFlags : std::uint16_t {
  JUST_OP = 0,  // Only operator -- no arguments
  OP_ARGS = 1,  // Operator takes arguments
  OP_SIZE = 2,  // Operator takes a size
};

struct OpStr {
  std::string_view str;
  std::uint16_t info;
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

class ListingFileImpl : public ListingFile {
 public:
  explicit ListingFileImpl(std::fstream listFile)
      : listFile_(std::move(listFile)) {}

  ~ListingFileImpl() override { listFile_.close(); }

 protected:
  bool Write(std::string_view text) override {
    listFile_ << text;
    return listFile_.good();
  }

 private:
  std::fstream listFile_;
};

class NullListingFileImpl : public ListingFile {
 public:
  NullListingFileImpl() = default;

 protected:
  bool Write(std::string_view text) override { return true; }
};
}  // namespace

class ListingFileSink {
 public:
  explicit ListingFileSink(ListingFile* listing_file)
      : listing_file_(listing_file) {}

 private:
  ListingFile* listing_file_;

  friend void AbslFormatFlush(ListingFileSink* sink, absl::string_view text) {
    if (!sink->listing_file_->Write(text)) {
      throw std::runtime_error("Error writing list file");
    }
  }
};

std::unique_ptr<ListingFile> ListingFile::Open(
    std::filesystem::path sourceFileName) {
  std::fstream listFile;
  listFile.exceptions(std::fstream::failbit | std::fstream::badbit);
  listFile.open(sourceFileName, std::ios::out | std::ios::trunc);
  if (!listFile.good()) {
    throw std::runtime_error(
        absl::StrFormat("Can't open %s for listing", sourceFileName));
  }

  auto result = std::make_unique<ListingFileImpl>(std::move(listFile));

  return result;
}

std::unique_ptr<ListingFile> ListingFile::Null() {
  return std::make_unique<NullListingFileImpl>();
}

void ListingFile::ListOp(std::size_t offset, uint8_t theOp) {
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

void ListingFile::ListWord(std::size_t offset, uint16_t w) {
  ListAsCode(offset, "word\t$%x", w);
}

void ListingFile::ListByte(std::size_t offset, uint8_t b) {
  ListAsCode(offset, "byte\t$%x", b);
}

void ListingFile::ListOffset(std::size_t offset) {
  ListingNoCRLF("\t\t%5x\t", offset);
}

void ListingFile::ListText(std::size_t offset, std::string_view s) {
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

void ListingFile::ListingImpl(
    absl::FunctionRef<bool(absl::FormatRawSink)> print_func) {
  ListingFileSink sink(this);
  if (!print_func(&sink) || !absl::Format(&sink, "\n")) {
    throw std::runtime_error("Error writing list file");
  }
}

void ListingFile::ListAsCodeImpl(
    std::size_t offset,
    absl::FunctionRef<bool(absl::FormatRawSink)> print_func) {
  ListOffset(offset);

  ListingImpl(print_func);
}

void ListingFile::ListingNoCRLFImpl(
    absl::FunctionRef<bool(absl::FormatRawSink)> print_func) {
  ListingFileSink sink(this);
  if (!print_func(&sink)) {
    throw std::runtime_error("Error writing list file");
  }
}
}  // namespace codegen
