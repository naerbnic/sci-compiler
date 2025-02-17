//	alist.cpp

#include "scic/alist.hpp"

#include <cstddef>

#include "scic/anode.hpp"
#include "scic/listing.hpp"
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

void ANode::collectFixups(FixupContext*) {}

void ANode::emit(OutputFile*) {}

void ANode::list(ListingFile* listFile) {}

bool ANode::contains(ANode* node) { return this == node; }

bool ANode::optimize() { return false; }
