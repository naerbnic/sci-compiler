//	list.cpp		sc

#include "sol.hpp"

#include	"sc.hpp"

#include "list.hpp"

List::List()
{
	head = tail = cur = 0;
}

List::~List()
{
	clear();
}

void
List::clear()
{
	while (head)
		del(head);
}

void
List::add(
	LNode*	ln)
{
	ln->next = 0;
	ln->prev = tail;
	if (tail)
		tail->next = ln;
	tail = ln;
	if (!head)
		head = ln;
}

void
List::addFront(
	LNode*	ln)
{
	if (head)
		head->prev = ln;
	ln->next = head;

	head = ln;
	ln->prev = 0;

	if (!tail)
		tail = ln;
}

void
List::addAfter(
	LNode*	ln,
	LNode*	nn)
{
	if (!ln) {
		addFront(nn);
		return;
	}

	nn->next = ln->next;
	if (nn->next)
		nn->next->prev = nn;

	nn->prev = ln;
	ln->next = nn;

	if (ln == tail)
		tail = nn;
}

void
List::addBefore(
	LNode*	ln,
	LNode*	nn)
{
	if (!ln) {
		add(nn);
		return;
	}

	nn->next = ln;
	nn->prev = ln->prev;

	if (ln->prev)
		ln->prev->next = nn;
	ln->prev = nn;

	if (ln == head)
		head = nn;
}

void
List::remove(
	LNode*	ln)
{
	if (!ln->next)
		tail = ln->prev;
	else
		ln->next->prev = ln->prev;

	if (!ln->prev)
		head = ln->next;
	else
		ln->prev->next = ln->next;

	if (cur == ln)
		cur = ln->prev;
	if (!cur)
		cur = head;
}

void
List::del(
	LNode*	ln)
{
	remove(ln);
	delete ln;
}

LNode*
List::replaceWith(
	LNode*	ln,
	LNode*	nn)
{
	nn->next = ln->next;
	nn->prev = ln->prev;

	if (!nn->next)
		tail = nn;
	else
		nn->next->prev = nn;

	if (!nn->prev)
		head = nn;
	else
		nn->prev->next = nn;

	if (cur == ln)
		cur = nn;

	delete ln;

	return nn;
}

Bool
List::contains(
	LNode*	ln)
{
	if (!ln)
		return False;

	LNode *node;
	for (node = head; node && node != ln; node = node->next)
		;

	return node == ln;
}

LNode*
List::first()
{
	cur = head;
	return cur;
}

LNode*
List::next()
{
	if (cur)
		cur = cur->next;
	return cur;
}
