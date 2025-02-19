#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/fixup_list.hpp"
#include "scic/codegen/varlist.hpp"
#include "scic/listing.hpp"
#include "util/types/choice.hpp"
#include "util/types/forward_ref.hpp"

class CodeGenerator;
class FunctionBuilder;

// Internal types
class ANDispTable;

// The value of a literal. Either an integer, or a static string, represented
// as a pointer to an ANText.
using LiteralValue = util::Choice<int, ANText*>;

struct ProcedureName {
  explicit ProcedureName(std::string name) : procName(std::move(name)) {}

  std::string procName;
};

struct MethodName {
  MethodName(std::string objName, std::string methName)
      : objName(std::move(objName)), methName(std::move(methName)) {}

  std::string objName;
  std::string methName;
};

using FuncName = util::Choice<ProcedureName, MethodName>;

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
  friend class CodeGenerator;

  static std::unique_ptr<ObjectCodegen> Create(CodeGenerator* compiler,
                                               bool isObj, std::string name);

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

class LabelRef {
 public:
 private:
  friend class FunctionBuilder;
  LabelRef() = default;
  ForwardRef<ANLabel*> ref;
};

class FunctionBuilder {
 public:
  enum BinOp {
    // Math
    ADD,
    SUB,
    MUL,
    DIV,
    SHL,
    SHR,
    MOD,
    AND,
    OR,
    XOR,

    // comparisons
    GT,
    GE,
    LT,
    LE,
    EQ,
    NE,
    UGT,
    UGE,
    ULT,
    ULE,
  };

  enum BranchOp {
    BNT,
    BT,
    JMP,
  };

  // Returns an ANode for the function. This can be used to resolve the
  // location of the function during assembly.
  ANode* GetNode() const;

  // Returns a Code Block node for the function. Opcodes can be added to it
  // to implement the function.
  //
  // TODO: Replace this with builder functions.
  AOpList* GetOpList() const;

  // Creates a label reference that can be used to branch to and
  // resolve a label.
  LabelRef CreateLabelRef();

  // Add an annotation indicating that the code is at the given line
  // number.
  void AddLineAnnotation(std::size_t lineNum);

  // Pushes the accumulator value onto the stack.
  void AddPushOp();

  // Pushes the previous accumulator to the stack, from the last comparison
  // binary operator.
  void AddPushPrevOp();

  // Removes the top value from the stack. This does not change
  // the accumulator
  void AddTossOp();

  // Pushes the top value of the stack onto the stack.
  void AddDupOp();

  // Loads the given value into the accumulator.
  void AddLoadImmediate(LiteralValue value);

  // Add a binary operation, combining the top of the stack with
  // the current accumulator value.
  void AddBinOp(BinOp op);

  // Add a branch op. Whether the branch is taken depends on the
  // accumulator.
  //
  // Takes an argument for the target of the branch. The label
  // for the target does not need to be resolved at this point,
  // but it must be resolved before the code is assembled.
  void AddBranchOp(BranchOp op, LabelRef* target);

  // Add a label for the target of a branch.
  //
  // All refs used in AddBranchOp() must have AddLabel() be called
  // on them to be resolved before the code is assembled.
  void AddLabel(LabelRef* label);

  // Add a Return.
  void AddReturnOp();

 private:
  friend class CodeGenerator;
  FunctionBuilder(ANCodeBlk* root_node);
  ANCodeBlk* code_node_;
};

class CodeGenerator {
 public:
  static std::unique_ptr<CodeGenerator> Create();
  ~CodeGenerator();

  void Assemble(uint16_t scriptNum, ListingFile* listFile);

  void AddPublic(std::string name, std::size_t index,
                 ForwardRef<ANode*>* target);

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

  std::unique_ptr<FunctionBuilder> CreateFunction(
      FuncName name, std::optional<std::size_t> lineNum, std::size_t numTemps);

 private:
  friend class ObjectCodegen;

  CodeGenerator();

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