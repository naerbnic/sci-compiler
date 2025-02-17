#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

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
  bool IsInHeap(ANode* node);

  ANText* AddTextNode(std::string_view text);

  std::unique_ptr<CodeList> hunkList;

  VarList localVars;
  int lastLineNum;
  ANTable* dispTbl;
  AList* objList;

 private:
  std::unique_ptr<FixupList> heapList;
  AList* textList;
  std::map<std::string, ANText*, std::less<>> textNodes;
};

#endif