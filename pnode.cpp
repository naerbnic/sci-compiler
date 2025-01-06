//	pnode.cpp	sc

#include "sol.hpp"

#include	"sc.hpp"

#include	"parse.hpp"

PNode::~PNode()
{
	for (PNode* node = child; node; ) {
		PNode* tmp = node->next;
		delete node;
		node = tmp;
	}
}

PNode*
PNode::addChild(
	PNode*	node)
{
	// Add a child node to the parent node at the end of the linked list
	// of children.

	PNode* pn = child;

	if (!pn)
		child = node;
	else {
		while (pn->next)
			pn = pn->next;
		pn->next = node;
	}

	return node;
}

