//	alist.cpp

#include "alist.hpp"

#include <cassert>

#include "anode.hpp"
#include "listing.hpp"
#include "opcodes.hpp"
#include "output.hpp"
#include "sc.hpp"

AList* curList;
bool shrink;
bool noOptimize;

///////////////////////////////////////////////////
// Class AListIter
///////////////////////////////////////////////////
ANode* AListIter::get() { return (ANode*)iter_.get(); }
void AListIter::advance() { iter_.advance(); }
AListIter::operator bool() { return bool(iter_); }
ANode* AListIter::operator->() { return get(); }

std::unique_ptr<ANode> AListIter::remove(ANode* an) {
  return std::unique_ptr<ANode>(
      static_cast<ANode*>(iter_.remove(an).release()));
}
ANode* AListIter::replaceWith(ANode* an, std::unique_ptr<ANode> nn) {
  return static_cast<ANode*>(iter_.replaceWith(an, std::move(nn)));
}

ANOpCode* AListIter::findOp(uint32_t op) {
  ANOpCode* nn = (ANOpCode*)get()->next();

  if (nn)
    return nn->op == op ? nn : 0;
  else
    return 0;
}

bool AListIter::removeOp(uint32_t op) {
  ANode* an = findOp(op);
  if (an) remove(an);

  return an != 0;
}

///////////////////////////////////////////////////
// Class AList
///////////////////////////////////////////////////

ANOpCode* AList::nextOp(ANode* start) {
  assert(start != NULL);

  ANOpCode* nn;

  for (nn = (ANOpCode*)start->next(); nn && nn->op == OP_LABEL;
       nn = (ANOpCode*)nn->next());

  return nn;
}

size_t AList::size() {
  size_t s = 0;
  for (auto it = iter(); it; it.advance()) s += it->size();
  return s;
}

void AList::emit(OutputFile* out) {
  for (auto it = iter(); it; it.advance()) {
    curOfs = it->offset;
    if (listCode) it->list();
    it->emit(out);
  }
}

size_t AList::setOffset(size_t ofs) {
  for (auto it = iter(); it; it.advance()) ofs = it->setOffset(ofs);

  return ofs;
}

void AList::optimize() {
  // Scan the assembly nodes, doing some peephole-like optimization.

  if (noOptimize) return;

  for (auto it = iter(); it; it.advance())
    while (it->optimize());
}

///////////////////////////////////////////////////
// Class FixupList
///////////////////////////////////////////////////

FixupList::FixupList() {}

FixupList::~FixupList() { clear(); }

void FixupList::clear() {
  AList::clear();
  fixups.clear();

  // All fixup lists have a word node at the start which is the offset
  // to the fixup table.
  newNode<ANWord>(0);
}

size_t FixupList::setOffset(size_t ofs) {
  fixOfs = AList::setOffset(ofs);
  return fixOfs;
}

void FixupList::initFixups() {
  // Set offset to fixup table.  If the table is on an odd boundary,
  // adjust to an even one.

  ((ANWord*)head)->value = fixOfs + (fixOfs & 1);
  fixups.clear();
}

void FixupList::listFixups() {
  curOfs = fixOfs;

  if (curOfs & 1) {
    ListByte(0);
    ++curOfs;
  }

  Listing("\n\nFixups:");
  ListWord(fixups.size());
  curOfs += 2;

  for (size_t fixup : fixups) {
    ListWord(fixup);
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
  AList::emit(out);
  emitFixups(out);
}

///////////////////////////////////////////////////
// Class CodeList
///////////////////////////////////////////////////

void CodeList::optimize() {
  AList::optimize();

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
