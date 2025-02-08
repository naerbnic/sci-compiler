//	text.cpp		sc

#include "scic/text.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

TextList gText;

void TextList::init() {
  textList.clear();
  size = 0;
}

uint32_t TextList::find(std::string_view str) {
  // The value of a string is its offset in string space.

  Text* text = search(str);

  // If the text string has already been used, use its offset.
  // Otherwise, allocate and link a new node into the list.
  if (!text) text = add(str);

  return text->num;
}

uint16_t TextList::hash(std::string_view tp) {
  uint16_t hashVal = 0;

  for (char ch : tp) hashVal += ch;

  return hashVal;
}

Text* TextList::add(std::string_view str) {
  // Add a string to text space

  // Allocate the text node and link it into the list.
  auto np_owned = std::make_unique<Text>();
  auto* np = np_owned.get();
  textList.push_back(std::move(np_owned));

  // Save the string and increment the offset in text space by
  // its length.  Get the hash value of the string.
  np->num = size;
  size += str.length() + 1;

  np->str = str;
  np->hashVal = hash(str);

  return np;
}

Text* TextList::search(std::string_view str) const {
  // Return the offset in string space of 'str' if it is already in string
  // space, 0 otherwise.

  uint16_t hashVal = hash(str);
  for (Text* tp : items()) {
    if (hashVal == tp->hashVal && str == tp->str) return tp;
  }
  return nullptr;
}
