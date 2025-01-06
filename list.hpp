//	list.hpp		sc
// 	definition of a list class

#if !defined(LIST_HPP)
#define LIST_HPP

struct LNode
{
	// Class LNode is the base class for any class to be linked in a doubly
	// linked list descended from List.

	virtual	~LNode() {};

	LNode*	next;
	LNode*	prev;
};

class List
{
	// Class List implements a doubly linked list.
public:
	List();
	~List();

	void		clear();
					// Uses del() to remove each element from the list and delete it.

	void		add(LNode* ln);
					// Add ln to the tail of the list.

	void		addFront(LNode* ln);
					// Add ln to the head of the list.

	void		addAfter(LNode* ln, LNode* nn);
					// Add node nn after node ln in the list.

	void		addBefore(LNode* ln, LNode* nn);
					// Add node nn before node ln in the list.

	void		remove(LNode* ln);
					// Remove node ln from the list.

	void		del(LNode* ln);
					// Remove node ln from the list and then delete it.

	LNode*	replaceWith(LNode* ln, LNode* nn);
					// Replace node ln with node nn, return a pointer to node nn.

	Bool		contains(LNode* ln);
					// Return True if the list contains node ln, False otherwise.

	LNode*	first();
					// Starts iteration of the list by setting the current iterator
					// position (property 'cur') to the head of the list and
					// returning the head.

	LNode*	next();
					// Once the iterator has been initialized with first(), return
					// the next node in the list and advance the current pointer
					// for the iterator.
protected:
	LNode	*head;		// head of the list
	LNode *tail;		// tail of the list
	LNode	*cur;			// current position of iterator in list
};

#endif

