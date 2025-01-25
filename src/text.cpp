//	text.cpp		sc

#include "text.hpp"

#include "jeff.hpp"
#include "sc.hpp"
#include "sol.hpp"
#include "string.hpp"

TextList text;

void TextList::init() {
  Text* next;
  for (Text* text = head; text;) {
    next = text->next;
    delete text;
    text = next;
  }

  head = tail = 0;
  size = 0;
}

uint32_t TextList::find(const char* str) {
  // The value of a string is its offset in string space.

  Text* text;

  // If the text string has already been used, use its offset.
  // Otherwise, allocate and link a new node into the list.
  if (!(text = search(str))) text = add(str);

  return text->num;
}

uword TextList::hash(const char* tp) {
  uword hashVal;

  for (hashVal = 0; *tp; ++tp) hashVal += *tp;

  return hashVal;
}

Text* TextList::add(const char* str) {
  // Add a string to text space

  Text* np;

  // Allocate the text node and link it into the list.
  np = new Text;
  if (!tail)
    head = tail = np;
  else {
    tail->next = np;
    tail = np;
  }

  // Save the string and increment the offset in text space by
  // its length.  Get the hash value of the string.
  np->num = size;
  size += strlen(str) + 1;

  np->str = newStr(str);
  np->hashVal = hash(str);

  return np;
}

Text* TextList::search(const char* str) const {
  // Return the offset in string space of 'str' if it is already in string
  // space, 0 otherwise.

  uword hashVal = hash(str);
  Text* tp;
  for (tp = head; tp && (tp->hashVal != hashVal || str != tp->str);
       tp = tp->next);

  return tp;
}
