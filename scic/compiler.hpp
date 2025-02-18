#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/anode_impls.hpp"
#include "scic/fixup_list.hpp"
#include "scic/listing.hpp"
#include "scic/varlist.hpp"
#include "util/types/forward_ref.hpp"

class Compiler;

// Internal types
class ANDispTable;

using LiteralValue = std::variant<int, ANText*>;

class ObjectCodegen {
 public:
  // Returns an ANode that will resolve to the object pointer.
  ANode* GetObjNode() const;

  // Append properties to the object.
  //
  // The order of these calls is significant.  The properties are
  // appended in the order they are added.
  void AppendProperty(std::string name, std::uint16_t selectorNum,
                      LiteralValue value);
  void AppendPropTableProperty(std::string name, std::uint16_t selectorNum);
  void AppendMethodTableProperty(std::string name, std::uint16_t selectorNum);

  // Append methods to the object.
  //
  // The order of these calls is significant.  The methods are appended
  // in the order they are added.
  void AppendMethod(std::string name, std::uint16_t selectorNum,
                    ANCodeBlk* code);

 private:
  friend class Compiler;

  static std::unique_ptr<ObjectCodegen> Create(Compiler* compiler, bool isObj,
                                               std::string name);

  ObjectCodegen(bool isObj, std::string name, ANObject* propListMarker,
                ANTable* props, ANObject* objDictMarker, ANObjTable* propDict,
                ANode* methDictStart, ANObjTable* methDict);

  void AppendPropDict(std::uint16_t selectorNum);

  // True iff this is an object, not a class.
  bool isObj_;

  std::string name_;
  // The prop-list object marker.
  ANObject* propListMarker_;
  ANTable* props_;

  ANObject* objDictMarker_;
  ANObjTable* propDict_;
  ANode* methDictStart_;
  ANObjTable* methDict_;

  bool wrotePropDict_ = false;
  bool wroteMethDict_ = false;
};

class Compiler {
 public:
  static std::unique_ptr<Compiler> Create();
  ~Compiler();

  void Assemble(uint16_t scriptNum, ListingFile* listFile);

  void AddPublic(std::string name, std::size_t index,
                 ForwardReference<ANode*>* target);

  bool IsInHeap(ANode const* node);

  ANText* AddTextNode(std::string_view text);

  // Returns the current number of variables.
  std::size_t NumVars() const;

  // Sets the variable at the given index to the given text.
  //
  // Returns false if the variable has already been set.
  bool SetVar(std::size_t varNum, LiteralValue value);

  std::unique_ptr<ObjectCodegen> CreateObject(std::string name);
  std::unique_ptr<ObjectCodegen> CreateClass(std::string name);

  ANCodeBlk* CreateProcedure(std::string name);
  ANCodeBlk* CreateMethod(std::string objName, std::string name);

 private:
  friend class ObjectCodegen;

  Compiler();

  void InitAsm();

  bool active;
  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<FixupList> hunkList;
  VarList localVars;
  ANDispTable* dispTable;
  ANodeList* objPropList;
  ANodeList* objDictList;
  ANodeList* codeList;
  ANodeList* textList;
  std::map<std::string, ANText*, std::less<>> textNodes;
};

#endif