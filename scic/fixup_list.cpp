#include "scic/fixup_list.hpp"

#include <cstddef>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/config.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"

///////////////////////////////////////////////////
// Class FixupList
///////////////////////////////////////////////////

FixupList::FixupList() {}

FixupList::~FixupList() { clear(); }

void FixupList::clear() {
  list_.list_.clear();
  fixups.clear();

  // All fixup lists have a word node at the start which is the offset
  // to the fixup table.
  newNode<ANWord>(0);
}

size_t FixupList::setOffset(size_t ofs) {
  fixOfs = list_.setOffset(ofs);
  return fixOfs;
}

void FixupList::initFixups() {
  // Set offset to fixup table.  If the table is on an odd boundary,
  // adjust to an even one.

  ((ANWord*)list_.list_.frontPtr())->value = fixOfs + (fixOfs & 1);
  fixups.clear();
}

void FixupList::listFixups(ListingFile* listFile) {
  std::size_t curOfs = fixOfs;

  if ((curOfs & 1) != 0U) {
    listFile->ListByte(curOfs, 0);
    ++curOfs;
  }

  listFile->Listing("\n\nFixups:");
  listFile->ListWord(curOfs, fixups.size());
  curOfs += 2;

  for (auto const& fixup : fixups) {
    listFile->ListWord(curOfs, fixup.offset());
    curOfs += 2;
  }
}

void FixupList::emitFixups(OutputFile* out) {
  if (fixOfs & 1) out->WriteByte(0);

  out->WriteWord(fixups.size());
  for (auto fixup : fixups) out->WriteWord(fixup.offset());
}

void FixupList::addFixup(size_t ofs) {
  fixups.push_back(Offset{
      .node_base = nullptr,
      .rel_offset = ofs,
  });
}

void FixupList::addFixup(ANode* node, std::size_t rel_ofs) {
  fixups.push_back(Offset{
      .node_base = node,
      .rel_offset = rel_ofs,
  });
}

void FixupList::list(ListingFile* listFile) {
  for (auto& node : list_.list_) {
    node.list(listFile);
  }
  listFixups(listFile);
}

void FixupList::emit(FixupContext* fixup_ctxt, OutputFile* out) {
  initFixups();
  list_.emit(fixup_ctxt, out);
  emitFixups(out);
}

///////////////////////////////////////////////////
// Class CodeList
///////////////////////////////////////////////////

void CodeList::optimize() {
  if (!gConfig->noOptimize) {
    for (auto it = list_.iter(); it; ++it)
      while (it->optimize())
        ;
  }

  // Make a first pass, resolving offsets and converting to byte offsets
  // where possible.
  gShrink = true;
  setOffset(0);

  // Continue resolving and converting to byte offsets until we've shrunk
  // the code as far as it will go.
  size_t curLen = 0, oldLen;
  do {
    oldLen = curLen;
    curLen = setOffset(0);
  } while (oldLen > curLen);

  // Now stabilize the code and offsets by resolving without allowing
  // conversion to byte offsets.
  gShrink = false;
  do {
    oldLen = curLen;
    curLen = setOffset(0);
  } while (oldLen != curLen);
}