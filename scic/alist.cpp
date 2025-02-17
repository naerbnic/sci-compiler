//	alist.cpp

#include "scic/alist.hpp"

#include <cassert>
#include <cstddef>
#include <ranges>

#include "scic/anode.hpp"
#include "scic/listing.hpp"
#include "scic/opcodes.hpp"
#include "scic/output.hpp"

bool gShrink;

///////////////////////////////////////////////////
// Class ANode
///////////////////////////////////////////////////

size_t ANode::size() { return 0; }

size_t ANode::setOffset(size_t ofs) {
  offset = ofs;
  return ofs + size();
}

void ANode::emit(FixupContext*, OutputFile*) {}

void ANode::list(ListingFile* listFile) {}

bool ANode::optimize() { return false; }

///////////////////////////////////////////////////
// Class AOpList
///////////////////////////////////////////////////

ANOpCode* AOpList::nextOp(ANOpCode* start) {
  assert(start != nullptr);

  for (auto& opcode : std::ranges::subrange(find(start).next(), end())) {
    if (opcode.op != OP_LABEL) {
      return &opcode;
    }
  }

  return nullptr;
}
