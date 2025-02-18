#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
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

  // Returns the current number of variables.
  std::size_t NumVars() const;

  // Sets the variable at the given index to the given text.
  //
  // Returns false if the variable has already been set.
  bool SetTextVar(std::size_t varNum, ANText* text);

  // Sets the variable at the given index to the given int.
  //
  // Returns false if the variable has already been set.
  bool SetIntVar(std::size_t varNum, int value);

  ANodeList* objPropList;
  ANodeList* objDictList;
  ANodeList* codeList;

 private:
  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<FixupList> hunkList;
  VarList localVars;
  ANTable* dispTbl;
  ANodeList* textList;
  std::map<std::string, ANText*, std::less<>> textNodes;
};

#endif