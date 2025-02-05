//	pnode.cpp	sc

#include "parse.hpp"

PNode* PNode::addChild(std::unique_ptr<PNode> node) {
  auto* ptr = node.get();
  children.push_back(std::move(node));
  return ptr;
}
