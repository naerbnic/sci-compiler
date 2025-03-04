#include "anode.hpp"

#include <cstddef>
#include <cstdint>

#include "scic/codegen/listing.hpp"
#include "scic/codegen/output.hpp"

namespace codegen {

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

void ANode::emit(OutputWriter*) const {}

void ANode::list(ListingFile* listFile) const {}

bool ANode::contains(ANode const* node) const { return this == node; }

bool ANode::optimize() { return false; }

///////////////////////////////////////////////////
// Class ANOpCode
///////////////////////////////////////////////////

ANOpCode::ANOpCode(std::uint32_t o) : op(o) {}

size_t ANOpCode::size() const { return 1; }

void ANOpCode::list(ListingFile* listFile) const {
  listFile->ListOp(*offset, op);
}

void ANOpCode::emit(OutputWriter* out) const { out->WriteOp(op); }

}  // namespace codegen
