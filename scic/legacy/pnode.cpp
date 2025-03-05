//	pnode.cpp	sc

#include "scic/legacy/pnode.hpp"

#include <memory>
#include <utility>

#include "scic/legacy/input.hpp"

PNode::PNode(pn_t t)
    : sym(0), val(0), type(t), lineNum(gInputState.GetTopLevelLineNum()) {}

PNode* PNode::addChild(std::unique_ptr<PNode> node) {
  auto* ptr = node.get();
  children.push_back(std::move(node));
  return ptr;
}
