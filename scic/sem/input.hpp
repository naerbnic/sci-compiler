#ifndef SEM_INPUT_HPP
#define SEM_INPUT_HPP

#include <vector>

#include "scic/parsers/sci/ast.hpp"
namespace sem {

struct Input {
  std::vector<parsers::sci::Item> global_items;

  struct Module {
    std::vector<parsers::sci::Item> module_items;
  };

  std::vector<Module> modules;
};

}  // namespace sem

#endif