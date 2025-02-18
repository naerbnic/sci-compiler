#include "anode.hpp"

#include <cstddef>

#include "scic/listing.hpp"

///////////////////////////////////////////////////
// Class ANode
///////////////////////////////////////////////////

size_t ANode::size() const { return 0; }

size_t ANode::setOffset(size_t ofs) {
  offset = ofs;
  return ofs + size();
}

bool ANode::tryShrink() { return false; };

void ANode::collectFixups(FixupContext*) const {}

void ANode::emit(OutputFile*) const {}

void ANode::list(ListingFile* listFile) const {}

bool ANode::contains(ANode const* node) const { return this == node; }

bool ANode::optimize() { return false; }