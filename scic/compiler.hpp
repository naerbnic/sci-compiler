#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
#include <memory>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/fixup_list.hpp"
#include "scic/listing.hpp"
#include "scic/varlist.hpp"

struct Compiler : public FixupContext {
  Compiler();
  ~Compiler();

  bool HeapHasNode(ANode* node) const override;
  void AddFixup(std::size_t ofs) override;

  void InitAsm();
  void Assemble(ListingFile* listFile);

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;

  VarList localVars;
  int lastLineNum;
  ANTable* dispTbl;
  ANWord* numDispTblEntries;
};

#endif