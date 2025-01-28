//	text.hpp		sc
//		definitions of text structures

#ifndef TEXT_HPP
#define TEXT_HPP

#include <ranges>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

struct Text {
  // Node for the message/string list.

  Text() : num(0), hashVal(0) {}

  int num;           // Offset in near string space
  std::string str;   // Pointer to the string itself
  uint16_t hashVal;  // Hashed value of the string
};

class TextList {
 public:
  void init();
  uint32_t find(std::string_view);

  auto items() const {
    return std::ranges::transform_view(
        textList, [](const auto& item) { return item.get(); });
  }

 protected:
  std::vector<std::unique_ptr<Text>> textList;
  size_t size;  // Size of text in list

  Text* add(std::string_view);
  Text* search(std::string_view) const;

  static uint16_t hash(std::string_view);
};

extern TextList text;

#endif
