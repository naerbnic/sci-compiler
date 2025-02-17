#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <memory>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/fixup_list.hpp"
#include "scic/listing.hpp"
#include "scic/public.hpp"
#include "scic/varlist.hpp"

struct Compiler {
  Compiler();
  ~Compiler();

  void InitAsm();
  void Assemble(ListingFile* listFile);

  void MakeDispatch(PublicList const& publicList);

  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<CodeList> hunkList;

  VarList localVars;
  int lastLineNum;
  ANTable* dispTbl;
};

#endif