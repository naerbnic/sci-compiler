#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
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

  VarList localVars;
  int lastLineNum;
  ANTable* dispTbl;
  ANodeList* objPropList;
  ANodeList* objDictList;
  ANodeList* codeList;

 private:
  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<FixupList> hunkList;
  ANodeList* textList;
  std::map<std::string, ANText*, std::less<>> textNodes;
};

#endif