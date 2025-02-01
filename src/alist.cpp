//	alist.cpp

#include "alist.hpp"

#include <cassert>

#include "anode.hpp"
#include "listing.hpp"
#include "opcodes.hpp"
#include "output.hpp"
#include "sc.hpp"

bool shrink;
bool noOptimize;

///////////////////////////////////////////////////
// Class ANode
///////////////////////////////////////////////////

size_t ANode::size() { return 0; }

size_t ANode::setOffset(size_t ofs) {
  offset = ofs;
  return ofs + size();
}

void ANode::emit(OutputFile*) {}

void ANode::list() {}

bool ANode::optimize() { return false; }

///////////////////////////////////////////////////
// Class AListIter
///////////////////////////////////////////////////
ANode* AListIter::get() const { return (ANode*)iter_.get(); }
void AListIter::advance() { ++iter_; }
AListIter::operator bool() { return bool(iter_); }
ANode* AListIter::operator->() { return get(); }
AListIter AListIter::next() const {
  auto it = *this;
  it.advance();
  return it;
}

std::unique_ptr<ANOpCode> AListIter::remove() { return iter_.remove(); }

ANOpCode* AListIter::replaceWith(std::unique_ptr<ANOpCode> nn) {
  iter_.replaceWith(std::move(nn));
  return iter_.get();
}

bool AListIter::isOp(uint32_t op) const {
  ANOpCode* nn = (ANOpCode*)get();
  if (!nn) {
    return false;
  }
  return nn->op == op;
}

///////////////////////////////////////////////////
// Class AList
///////////////////////////////////////////////////

size_t AList::size() {
  size_t s = 0;
  for (auto it = iter(); it; ++it) s += it->size();
  return s;
}

void AList::emit(OutputFile* out) {
  for (auto it = iter(); it; ++it) {
    curOfs = it->offset;
    if (listCode) it->list();
    it->emit(out);
  }
}

size_t AList::setOffset(size_t ofs) {
  for (auto it = iter(); it; ++it) ofs = it->setOffset(ofs);

  return ofs;
}

///////////////////////////////////////////////////
// Class AList
///////////////////////////////////////////////////

ANOpCode* AOpList::nextOp(ANOpCode* start) {
  assert(start != NULL);

  for (ANode& node :
       std::ranges::subrange(list_.findIter(start).next(), list_.end())) {
    auto* nn = (ANOpCode*)&node;
    if (nn->op != OP_LABEL) {
      return nn;
    }
  }

  return nullptr;
}

size_t AOpList::size() {
  size_t s = 0;
  for (auto it = iter(); it; it.advance()) s += it->size();
  return s;
}

void AOpList::emit(OutputFile* out) {
  for (auto it = iter(); it; it.advance()) {
    // curOfs = it->offset;
    if (listCode) it->list();
    it->emit(out);
  }
}

size_t AOpList::setOffset(size_t ofs) {
  for (auto it = iter(); it; it.advance()) ofs = it->setOffset(ofs);

  return ofs;
}

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

void FixupList::listFixups() {
  std::size_t curOfs = fixOfs;

  if (curOfs & 1) {
    ListByte(curOfs, 0);
    ++curOfs;
  }

  Listing("\n\nFixups:");
  ListWord(curOfs, fixups.size());
  curOfs += 2;

  for (size_t fixup : fixups) {
    ListWord(curOfs, fixup);
    curOfs += 2;
  }
}

void FixupList::emitFixups(OutputFile* out) {
  if (listCode) listFixups();

  if (fixOfs & 1) out->WriteByte(0);

  out->WriteWord(fixups.size());
  for (size_t fixup : fixups) out->WriteWord(fixup);
}

void FixupList::addFixup(size_t ofs) { fixups.push_back(ofs); }

void FixupList::emit(OutputFile* out) {
  initFixups();
  list_.emit(out);
  emitFixups(out);
}

///////////////////////////////////////////////////
// Class CodeList
///////////////////////////////////////////////////

void CodeList::optimize() {
  if (!noOptimize) {
    for (auto it = list_.iter(); it; ++it)
      while (it->optimize());
  }

  // Make a first pass, resolving offsets and converting to byte offsets
  // where possible.
  shrink = true;
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
  shrink = false;
  do {
    oldLen = curLen;
    curLen = setOffset(0);
  } while (oldLen != curLen);
}
