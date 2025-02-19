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
#include <vector>

#include "scic/codegen/alist.hpp"
#include "scic/codegen/anode.hpp"
#include "scic/codegen/anode_impls.hpp"
#include "scic/codegen/fixup_list.hpp"
#include "scic/codegen/target.hpp"
#include "scic/listing.hpp"
#include "util/types/choice.hpp"
#include "util/types/forward_ref.hpp"

// Forward declarations
class CodeGenerator;
class FunctionBuilder;
class ObjectCodegen;
class ANVars;

// Internal types
class ANDispTable;
class Var;

enum class SciTarget {
  SCI_1_1,
  SCI_2,
};

enum class Optimization {
  OPTIMIZE,
  NO_OPTIMIZE,
};

class TextRef {
 public:
  std::string_view text() const { return ref_->text; }
  bool operator==(TextRef const& other) const = default;

 private:
  friend class CodeGenerator;
  friend class FunctionBuilder;
  friend class ObjectCodegen;
  friend class ANVars;
  explicit TextRef(ANText* ref) : ref_(ref) {}
  ANText* ref_;
};

class PtrRef {
 public:
  bool is_resolved() const { return ref_.IsResolved(); }

 private:
  friend class CodeGenerator;
  friend class FunctionBuilder;
  friend class ObjectCodegen;

  ForwardRef<ANode*> ref_;
};

// The value of a literal. Either an integer, or a static string, represented
// as a pointer to an ANText.
using LiteralValue = util::Choice<int, TextRef>;

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
  void AppendMethod(std::string name, std::uint16_t selectorNum, PtrRef* code);

 private:
  friend class CodeGenerator;

  static std::unique_ptr<ObjectCodegen> Create(CodeGenerator* compiler,
                                               bool isObj, std::string name,
                                               ForwardRef<ANode*>* ref);

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
  ForwardRef<ANLabel*> ref_;
};

class FunctionBuilder {
 public:
  enum UnOp {
    NEG,
    NOT,
    BNOT,
  };

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

  enum VarType {
    GLOBAL,
    LOCAL,
    PARAM,
    TEMP,
  };

  // What kind of operation to use on a slot (property or variable)
  enum ValueOp {
    // Load the value to the accumulator.
    LOAD,
    // Store the top of the stack to the slot.
    STORE,
    // Increment the slot, and load the value to the accumulator.
    INC,
    // Decrement the slot, and load the value to the accumulator.
    DEC,
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

  // Pushes an immediate value onto the stack.
  void AddPushImmediate(int value);

  // Pushes the previous accumulator to the stack, from the last comparison
  // binary operator.
  void AddPushPrevOp();

  // Removes the top value from the stack. This does not change
  // the accumulator
  void AddTossOp();

  // Pushes the top value of the stack onto the stack.
  void AddDupOp();

  // Pushes the rest of the arguments onto the stack, starting from the
  // given parameter index.
  void AddRestOp(std::size_t arg_index);

  // Loads the given value into the accumulator.
  void AddLoadImmediate(LiteralValue value);

  // Loads an offset to the given entity into the accumulator.
  //
  //
  void AddLoadOffsetTo(PtrRef* ptr,
                       std::optional<std::string> name = std::nullopt);

  // Load the effective address of the given variable.
  //
  // Loads the address with the offset of the variable with the given type
  // into the accumulator.
  //
  // If add_accum_index is true, it will also add the accumulator index to the
  // offset before loading the address.
  void AddLoadVarAddr(VarType var_type, std::size_t offset,
                      bool add_accum_index, std::optional<std::string> name);

  // Add a variable access operation.
  //
  // All of these use the parameters. Loads put the result in the accumulator.
  // Store will always pop the stored value off the top of the stack.
  //
  // If add_accum_index is true, it will also add the accumulator index to the
  // offset before performing the access.
  void AddVarAccess(VarType var_type, ValueOp value_op, std::size_t offset,
                    bool add_accum_index, std::optional<std::string> name);

  // Add a property access operation.
  //
  // The value ops have the same semantics as AddVarAccess.
  //
  // It is not possible to index a property with the accumulator.
  void AddPropAccess(ValueOp value_op, std::size_t offset,
                     std::optional<std::string> name);

  // Loads a pointer to the class with the given species into the accumulator.
  void AddLoadClassOp(std::string name, std::size_t species);

  // Loads a pointer to the current object into the accumulator.
  void AddLoadSelfOp();

  // Add a unary operation, operating on the current accumulator.
  void AddUnOp(UnOp op);

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

  // Adds a call to the given procedure.
  void AddProcCall(std::string name, std::size_t numArgs, PtrRef* target);

  void AddExternCall(std::string name, std::size_t numArgs,
                     std::size_t script_num, std::size_t entry);

  void AddKernelCall(std::string name, std::size_t numArgs, std::size_t entry);

  // Send methods.
  void AddSend(std::size_t numArgs);
  void AddSelfSend(std::size_t numArgs);
  void AddSuperSend(std::string name, std::size_t numArgs, std::size_t species);

  // Add a Return.
  void AddReturnOp();

 private:
  friend class CodeGenerator;
  FunctionBuilder(SciTargetStrategy const* target, ANCodeBlk* root_node);
  SciTargetStrategy const* target_;
  ANCodeBlk* code_node_;
};

class CodeGenerator {
 public:
  static std::unique_ptr<CodeGenerator> Create(SciTarget target,
                                               Optimization opt);
  ~CodeGenerator();

  void Assemble(uint16_t scriptNum, ListingFile* listFile);

  PtrRef CreatePtrRef();

  void AddPublic(std::string name, std::size_t index, PtrRef* target);

  bool IsInHeap(ANode const* node);

  TextRef AddTextNode(std::string_view text);

  // Returns the current number of variables.
  std::size_t NumVars() const;

  // Sets the variable at the given index to the given text.
  //
  // Returns false if the variable has already been set.
  bool SetVar(std::size_t varNum, LiteralValue value);

  std::unique_ptr<ObjectCodegen> CreateObject(std::string name, PtrRef* ref);
  std::unique_ptr<ObjectCodegen> CreateClass(std::string name, PtrRef* ref);

  std::unique_ptr<FunctionBuilder> CreateFunction(
      FuncName name, std::optional<std::size_t> lineNum, std::size_t numTemps,
      PtrRef* ptr_ref);

 private:
  friend class ObjectCodegen;

  CodeGenerator();

  void InitAsm();

  SciTargetStrategy const* sci_target;
  Optimization opt;
  bool active;
  std::unique_ptr<FixupList> heapList;
  std::unique_ptr<FixupList> hunkList;
  std::vector<Var> localVars;
  ANDispTable* dispTable;
  ANodeList* objPropList;
  ANodeList* objDictList;
  ANodeList* codeList;
  ANodeList* textList;
  std::map<std::string, ANText*, std::less<>> textNodes;
};

#endif