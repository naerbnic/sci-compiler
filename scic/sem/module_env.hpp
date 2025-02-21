#ifndef SEM_MODULE_ENV_HPP
#define SEM_MODULE_ENV_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "scic/sem/symbol.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/choice.hpp"

namespace sem {

class Text {
 public:
  explicit Text(util::RefStr text) : text_(std::move(text)) {}

 private:
  util::RefStr text_;
};

// The value of an entry, either a variable or a property.
class EntryValue : util::ChoiceBase<EntryValue, Text const*, int> {};

class ProcBody {
 public:
  struct Temporary {
    SymbolId name;
    std::size_t size;
  };

 private:
  std::vector<SymbolId> params_;
  std::vector<Temporary> temporaries_;
  // We need some representation for the assembled code. Do this later, probably
  // using ANode from the main compiler.
};

class Object {
 public:
  enum Kind {
    OBJECT,
    CLASS,
  };

  struct Property {
    SelectorId selector;
    EntryValue value;
  };

  struct Method {
    SelectorId selector;
    ProcBody body;
  };

  Object(Kind kind, std::optional<ClassSpecies> super = std::nullopt)
      : kind_(kind), super_(super) {}

  Property* AddProperty(SelectorId selector, EntryValue value);
  Method* AddMethod(SelectorId selector, ProcBody body);

  Property* FindProperty(SelectorId selector);
  Property const* FindProperty(SelectorId selector) const;

  Method* FindMethod(SelectorId selector);
  Method const* FindMethod(SelectorId selector) const;

 private:
  Kind kind_;
  std::optional<ClassSpecies> super_;
  std::vector<Property> properties_;
  std::vector<Method> methods_;
};

class TextTable {
 public:
  Text const* GetText(std::string_view name) {
    auto it = texts_.find(name);
    if (it != texts_.end()) {
      return &it->second;
    }

    auto [insert_it, inserted] = texts_.emplace(name, Text(util::RefStr(name)));
    if (!inserted) {
      throw std::runtime_error("Failed to insert text");
    }
    return &insert_it->second;
  }

 private:
  std::map<util::RefStr, Text, std::less<>> texts_;
};

class ProcTable {};

class VarsTable {};

class ObjectTable {
 public:
  Object* NewObject(SymbolId name, Object::Kind kind,
                    std::optional<ClassSpecies> super = std::nullopt);

 private:
};

}  // namespace sem

#endif