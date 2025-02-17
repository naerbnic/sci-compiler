#include "anode.hpp"

#include <cstddef>

///////////////////////////////////////////////////
// Class ANode
///////////////////////////////////////////////////

size_t ANode::size() { return 0; }

size_t ANode::setOffset(size_t ofs) {
  offset = ofs;
  return ofs + size();
}

bool ANode::tryShrink() { return false; };

void ANode::collectFixups(FixupContext*) {}

void ANode::emit(OutputFile*) {}

void ANode::list(ListingFile* listFile) {}

bool ANode::contains(ANode* node) { return this == node; }

bool ANode::optimize() { return false; }