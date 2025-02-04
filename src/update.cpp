//	update.cpp		sc
// 	update the database of class and selector information

#include "update.hpp"

#include <stdlib.h>
#include <string.h>

#include <filesystem>

#include "absl/strings/str_format.h"
#include "error.hpp"
#include "input.hpp"
#include "object.hpp"
#include "output.hpp"
#include "resource.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "symtbl.hpp"
#include "token.hpp"
#include "vocab.hpp"

bool gClassAdded;
bool gSelectorAdded;
std::filesystem::path gOutDir;
bool gWriteOffsets;

static uint8_t resHdr[] = {MemResVocab, 0};

static void WriteSelector();
static void WriteClassDefs();
static void WriteClasses();
static void WriteSelectorVocab();
static void PrintSubClasses(Class* op, int level, FILE* fp);

void UpdateDataBase() {
  if (gSelectorAdded) {
    WriteSelector();
    WriteSelectorVocab();
  }

  if (gClassAdded) {
    WriteClassDefs();
    WriteClasses();
  }

  gSelectorAdded = gClassAdded = false;
}

void WriteClassTbl() {
  // Write the file 'classtbl'.  This is an array, indexed by class number,
  // with two words for each class.  The first word is space to store the
  // class ID when the class is loaded.  The second word contains the script
  // number in which the class resides.

  static uint8_t resID[2] = {MemResVocab, 0};
#pragma pack(push, 1)
  struct ClassTblEntry {
    SCIUWord objID;
    SCIUWord scriptNum;
  };
#pragma pack(pop)
  static_assert(sizeof(ClassTblEntry) == 4, "Class table entry is wrong size");

  // Allocate storage for the class table.
  std::vector<ClassTblEntry> classTbl;
  classTbl.resize(gMaxClassNum + 1);

  // Now walk through the class symbol table, entering the script
  // number of each class in its proper place in the table.
  int index;
  for (auto* sym : gSyms.classSymTbl->symbols()) {
    if (sym->obj()->num != -1) {
      classTbl[sym->obj()->num].objID = 0;
      classTbl[sym->obj()->num].scriptNum = SCIUWord(sym->obj()->script);
    }
  }

  // Write the table out.
  std::string name = ResNameMake(MemResVocab, CLASSTBL_VOCAB);
  OutputFile out((gOutDir / name).string());
  out.Write(resID, 2);
  for (index = 0; index < gMaxClassNum + 1; ++index) {
    out.WriteWord(classTbl[index].objID);
    out.WriteWord(classTbl[index].scriptNum);
  }
}

void WritePropOffsets() {
  // Read the file "offsets.txt" and write out "offsets", a file with the
  // offsets (in words) of properties in given classes.  This allows for
  // considerably faster execution in the kernel.

  Object* cp;
  int offset;
  Selector* sel;
  Symbol* theSym;

  gTheFile = OpenFileAsInput("offsets.txt", true);

  std::string name = ResNameMake(MemResVocab, PROPOFS_VOCAB);
  OutputFile out((gOutDir / name).string());

  // Write out the resource header (this will be a vocabulary resource).
  out.Write(resHdr, sizeof resHdr);

  while (NewToken()) {
    theSym = gSyms.lookup(gSymStr);
    if (theSym->type != S_CLASS) {
      Error("Not a class: %s", gSymStr);
      GetToken();
      continue;
    }
    cp = theSym->obj();
    if (!LookupTok() || !(sel = cp->findSelectorByNum(gTokSym.val()))) {
      Error("Not a selector for class %s: %s", cp->sym->name(), gSymStr);
      continue;
    }

    offset = sel->ofs / 2;
    out.WriteWord(offset);
  }
}

static void WriteSelector() {
  FILE* fp;

  if (!(fp = fopen("selector", "w")))
    Panic("Can't open 'selector' for output.");
  fseek(fp, 0L, SEEK_SET);

  absl::FPrintF(fp, "(selectors\n");
  for (auto* sp : gSyms.selectorSymTbl->symbols())
    absl::FPrintF(fp, "\t%-20s %d\n", sp->name(), sp->val());

  absl::FPrintF(fp, ")\n");

  if (fclose(fp) == EOF) Panic("Error writing selector file");
}

static void WriteClassDefs() {
  FILE* fp;
  if (!(fp = fopen("classdef", "w")))
    Panic("Can't open 'classdef' for output.");

  int classNum = -1;
  for (Class* cp = NextClass(classNum); cp; cp = NextClass(classNum)) {
    classNum = cp->num;
    if (cp->num == -1) continue;  // This is RootObj, defined by compiler.

    absl::FPrintF(fp,
                  "(classdef %s\n"
                  "	script# %d\n"
                  "	class# %d\n"
                  "	super# %d\n"
                  "	file# \"%s\"\n\n",
                  cp->sym->name(), (SCIUWord)cp->script, (SCIUWord)cp->num,
                  (SCIUWord)cp->super, cp->file);

    // Get a pointer to the class' super-class.
    Class* sp = FindClass(cp->findSelector("-super-")->val);

    // Write out any new properties or properties which differ in
    // value from the superclass.
    absl::FPrintF(fp, "\t(properties\n");
    for (auto* tp : cp->selectors()) {
      if (IsProperty(tp) && sp->selectorDiffers(tp))
        absl::FPrintF(fp, "\t\t%s %d\n", tp->sym->name(), tp->val);
    }
    absl::FPrintF(fp, "\t)\n\n");

    // Write out any new methods or methods which have been redefined.
    absl::FPrintF(fp, "\t(methods\n");
    for (auto* tp : cp->selectors())
      if (IsMethod(tp) && sp->selectorDiffers(tp))
        absl::FPrintF(fp, "\t\t%s\n", tp->sym->name());
    absl::FPrintF(fp, "\t)\n");

    absl::FPrintF(fp, ")\n\n\n");
  }

  if (fclose(fp) == EOF) Panic("Error writing classdef");
}

static void WriteClasses() {
  FILE* fp;

  // Open the file 'classes', on which this heirarchy is printed.
  if (!(fp = fopen("classes", "w"))) Panic("Can't open 'classes' for output.");

  // Print the classes in heirarchical order.
  PrintSubClasses(gClasses[0], 0, fp);

  // Close the file.
  if (fclose(fp) == EOF) Panic("Error writing 'classes'");
}

static void PrintSubClasses(Class* sp, int level, FILE* fp) {
  // Print out this class' information.
  absl::FPrintF(fp, "%.*s%-*s;%s\n", 2 * level, "               ",
                20 - 2 * level, sp->sym->name(), sp->file);

  // Print information about this class' subclasses.
  ++level;
  for (Class* cp = sp->subClasses; cp; cp = cp->nextSibling)
    PrintSubClasses(cp, level, fp);
}

static void WriteSelectorVocab() {
  static char badSelMsg[] = "BAD SELECTOR";

  size_t ofs;
  uint32_t tblLen;

  std::string resName = ResNameMake(MemResVocab, SELECTOR_VOCAB);
  std::string fileName = (gOutDir / resName).string();

  // Compute the size of the table needed to hold offsets to all selectors,
  // allocate the table, and initialize it to point to the byte following
  // the table so that un-implemented selector values will have a string.
  tblLen = sizeof(SCIUWord) * (gMaxSelector + 2);
  auto tbl = std::make_unique<SCIUWord[]>(tblLen / sizeof(SCIUWord));
  ofs = tblLen;
  tbl[0] = SCIUWord(gMaxSelector);
  for (int i = 1; i <= gMaxSelector + 1; ++i) tbl[i] = SCIUWord(ofs);

  OutputFile out(fileName);

  // Write out the resource header (this will be a vocabulary resource).
  out.Write(resHdr, sizeof resHdr);

  // Seek to the beginning of the string area of the file and write the
  // bad selector string.
  out.SeekTo(tblLen + sizeof resHdr);
  ofs += out.Write(badSelMsg);

  // Now write out the names of all the other selectors and put their
  // offsets into the table.
  for (auto* sp : gSyms.selectorSymTbl->symbols()) {
    tbl[sp->val() + 1] = SCIUWord(ofs);
    ofs += out.Write(sp->name());
  }

  // Seek back to the table's position in the file and write it out.
  out.SeekTo(sizeof resHdr);
  for (size_t i = 0; i < tblLen / sizeof(SCIUWord); i++) out.WriteWord(tbl[i]);
}
