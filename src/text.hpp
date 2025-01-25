//	text.hpp		sc
//		definitions of text structures

#ifndef TEXT_HPP
#define TEXT_HPP

#include <string>

#include "sc.hpp"

struct Text {
  // Node for the message/string list.

  Text() : next(0), num(0), hashVal(0) {}

  Text* next;
  int num;          // Offset in near string space
  std::string str;  // Pointer to the string itself
  uword hashVal;    // Hashed value of the string
};

class TextList {
 public:
  void init();
  uint32_t find(const char*);

  Text* head;   // Pointer to start of text list
  Text* tail;   // Pointer to end of text list
  size_t size;  // Size of text in list

 protected:
  Text* add(const char*);
  Text* search(const char*) const;

  static uword hash(const char*);
};

extern TextList text;

#endif
