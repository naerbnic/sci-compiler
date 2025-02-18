//	anode.hpp
// 	assembly nodes

#ifndef ANODE_IMPLS_HPP
#define ANODE_IMPLS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "scic/alist.hpp"
#include "scic/anode.hpp"
#include "scic/common.hpp"
#include "scic/listing.hpp"
#include "scic/output.hpp"

// Sizes of parameters
#define OPSIZE 1
#define BYTESIZE 2
#define WORDSIZE 3

class ANComputedWord : public ANode {
 public:
  size_t size() override { return 2; }
  void list(ListingFile* listFile) override {
    listFile->ListWord(*offset, value());
  }
  void emit(OutputFile* out) override { out->WriteWord(value()); }

 protected:
  virtual SCIWord value() const = 0;
};

class ANOffsetWord : public ANComputedWord {
 public:
  ANOffsetWord(ANode* target, std::size_t rel_offset)
      : target(target), rel_offset(rel_offset) {}

  ANode* target;
  std::size_t rel_offset;

 protected:
  SCIWord value() const override { return *target->offset + rel_offset; }
};

class ANCountWord : public ANComputedWord {
 public:
  ANCountWord(ANodeList* target) : target(target) {}

  ANodeList* target;

 protected:
  SCIWord value() const override { return target->length(); }
};

struct ANDispatch : ANode
// The ANDispatch class serves as the members of the dispatch table to
// publicly defined procedures and objects.  There is one generated for
// each line in the (public ...) statement in SCI.  These are added to
// the 'dispTbl' dispatch table, set up in InitAsm().
{
  size_t size() override;
  void list(ListingFile* listFile) override;
  void collectFixups(FixupContext*) override;
  void emit(OutputFile*) override;

  std::optional<std::string> name;
  ANode* target;
};

struct ANWord : ANode
// An ANWord is the node used to generate an arbitrary word.  It is often
// used as a member of an ANTable, but can also appear directly in the
// objcode output.  The property 'value' contains the value of the word.
{
  ANWord(int v = 0);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  int value;
};

struct ANTable : ANComposite<ANode>
// An ANTable is a collection of ANodes that go together in some way.
// It is used to build objects, dispatch tables, etc.  When created, it
// saves a pointer to the list currently being added to in its 'oldList'
// property, then sets the current list to its own internal list.  The
// finish() method must be called when the table is completed to restore
// the original list as the current one.
{
  ANTable(std::string nameStr);

  void list(ListingFile* listFile) override;

  std::string name;  // name of table (values follow)
};

struct ANObjTable : ANTable
// ANObjTable sub-classes ANTable to have the table added before the first
// instance of code in the hunk list.
{
  ANObjTable(std::string nameStr);
};

struct Text;

struct ANText : ANode
// The ANText class represents a text string.
{
  ANText(std::string text);

  size_t setOffset(size_t ofs) override;  // set offset to ofs, return new ofs
  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  std::string text;
};

struct ANObject : ANode
// The ANObject class is the target of a reference to an instance or class.
// It generates nothing in the object code file.  The object itself is built
// up of ANTables containing properties, method dispatch vectors, etc.
{
  ANObject(std::string name);

  void list(ListingFile* listFile) override;

  std::string name;  // name of object
};

struct ANCodeBlk : ANComposite<ANOpCode>
// The ANCodeBlk class represents the code of a procedure or method.
// Like ANTable, it changes the list to which ANodes are added to its
// own internal list.  Therefore the finish() method must be called to
// reset the current list back to its original value when the code is
// complete.
{
  ANCodeBlk(std::string name);

  bool optimize() override;

  std::string name;  // name of procedure or method
};

struct ANMethCode : ANCodeBlk
// ANMethCode is just a listing-specific subclass of ANCodeBlk, which
// generates "Method" rather than "Procedure" in the listing.
{
  ANMethCode(std::string name, std::string obj_name);

  void list(ListingFile* listFile) override;

  std::string obj_name;  // name of object
};

struct ANProcCode : ANCodeBlk
// ANProcCode is just a listing-specific subclass of ANCodeBlk, which
// generates "Procedure" rather than "Method" in the listing.
{
  ANProcCode(std::string name) : ANCodeBlk(std::move(name)) {}

  void list(ListingFile* listFile);
};

struct ANProp : ANode
// ANProp is the base class representing the properties of an object.
// Its method uses the virtual methods desc() and value() to deal with
// the differences between the various property types.
{
  ANProp(std::string name);

  virtual std::string_view desc() = 0;  // return descriptive string
  virtual uint32_t value() = 0;         // return value of selector

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  std::string name;  // pointer to selector's symbol
};

struct ANIntProp : ANProp
// A subclass of ANProp which represents integer properties.
{
  ANIntProp(std::string name, int v) : ANProp(std::move(name)), val(v) {}

  std::string_view desc() override;  // return descriptive string
  uint32_t value() override;         // return value of selector

  int val;
};

struct ANOfsProp : ANProp
// A subclass of ANProp which represents an offset to an object table.
{
  ANOfsProp(std::string name, ANode* target = nullptr)
      : ANProp(std::move(name)), target(target) {}

  void collectFixups(FixupContext*) override;
  std::string_view desc() override;
  uint32_t value() override;

  ANode* target;
};

struct ANMethod : ANProp
// A subclass of ANProp which represents methods.
{
  ANMethod(std::string name, ANCodeBlk* mp);

  std::string_view desc() override;  // return descriptive string
  uint32_t value() override;         // return value of selector

  ANCodeBlk* method;
};

class ANLabel : public ANOpCode
// The ANLabel class is not really an opcode (since it generates no object
// code), but is the target of branches.  It is subclassed from ANOpCode for
// reasons of polymorphism (so that we can walk down lists of ANOpCodes
// including ANLabels).
// The property 'number' is the number of the label.  It is used in the
// listing so that branches can be mapped to targets.
// The property 'nextLabel' keeps track of the number of the next label
// to be generated by 'new'.  This is reset to zero with the 'reset()'
// method at the start of each procedure and method.
{
 public:
  ANLabel();

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t number;  // label number

  static void reset() { nextLabel = 0; }

 private:
  static uint32_t nextLabel;  // label number of next label
};

struct ANOpUnsign : ANOpCode
// The ANOpUnsign class is an ANOpcode which takes an unsigned integer
// as the argument to the opcode.
{
  ANOpUnsign(uint32_t o, uint32_t v);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t value;
  std::optional<std::string> name;
};

struct ANOpSign : ANOpCode
// The ANOpSign class is an ANOpcode which takes a signed integer as the
// argument to the opcode.
{
  ANOpSign(uint32_t o, int v);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  int value;
  std::optional<std::string> name;
};

struct ANOpExtern : ANOpCode
// The ANOpExtern class describes a call to an external proceedure.
{
  ANOpExtern(std::string name, int32_t m, uint32_t e);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  int32_t module;    // module # of destination
  uint32_t entry;    // entry # of destination
  uint32_t numArgs;  // number of arguments
  std::string name;
};

// The ANCall class describes a call to a procedure in the current module.
struct ANCall : ANOpCode {
  ANCall(std::string name);

  size_t size() override;
  bool tryShrink() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t numArgs;  // number of arguments
  std::string name;  // symbol of procedure being called
  ANode* target;     // target of call
};

struct ANBranch : ANOpCode
// The ANBranch class represents a branch opcode.  The 'target' property
// is the ANode (actually the ANLabel) to which to branch.
{
  ANBranch(uint32_t o);

  size_t size() override;
  bool tryShrink() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  ANLabel* target;
};

struct ANVarAccess : ANOpCode
// The ANVarAccess class represents access to variables.  The 'op' property
// (inherited from ANOpCode) contains the type of access (load or store),
// the 'addr' property the offset of the variable in the appropriate
// variable block (global, local, or temporary).
{
  ANVarAccess(uint32_t o, uint32_t a);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t addr;                    // variable address
  std::optional<std::string> name;  //  variable name
};

struct ANOpOfs : ANOpCode
// The ANOpOfs class gives the offset of a text string in
// its block of the object code.
{
  ANOpOfs(ANText* text);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void collectFixups(FixupContext*) override;
  void emit(OutputFile*) override;

  ANText* text;  // the offset
};

struct ANObjID : ANOpCode
// The ANObjID class represents a reference to an object.  In the object
// code, it is just the offset of the object within the code segment.
// In the interpreter this gets fixed up at load time so that the opcode
// generates the address of the object in the heap.
{
  ANObjID(int lineNum, std::string name);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void collectFixups(FixupContext*) override;
  void emit(OutputFile*) override;

  int lineNum;
  std::string name;
  ANode* target;
};

struct ANEffctAddr : ANVarAccess
// The ANEffctAddr class loads the accumulator with the address of
// a variable.  The type of variable is determined by the value of
// the property 'eaType'.
{
  ANEffctAddr(uint32_t o, uint32_t a, uint32_t t);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t eaType;  // type of access
};

struct ANSend : ANOpCode
// The ANSend class represents a send to an object.
{
  ANSend(uint32_t o);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t numArgs;
};

struct ANSuper : ANSend
// The ANSuper class represents a send to the superclass whose number is
// 'classNum'.
{
  ANSuper(std::string name, uint32_t c);

  size_t size() override;
  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;

  uint32_t classNum;
  std::string name;
};

struct ANFileName : ANOpCode {
  //	contains the name of this script's source file for debugging

  ANFileName(std::string name);

  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;
  size_t size() override;

 protected:
  std::string name;
};

struct ANLineNum : ANOpCode {
  //	contains the current line number

  ANLineNum(int num);

  void list(ListingFile* listFile) override;
  void emit(OutputFile*) override;
  size_t size() override;

 protected:
  int num;
};

#endif
