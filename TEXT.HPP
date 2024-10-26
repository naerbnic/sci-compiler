//	text.hpp		sc
//		definitions of text structures

#ifndef TEXT_HPP
#define TEXT_HPP

struct Text {
	// Node for the message/string list.

	Text() : next(0), num(0), str(0), hashVal(0) {}
	~Text() { delete[] str; }

	Text*		next;
	int		num;			// Offset in near string space
	char*		str;			// Pointer to the string itself
	uword		hashVal;		// Hashed value of the string
};

class TextList {
public:
	void	init();
	uint	find(char*);

	Text*		head;			// Pointer to start of text list
	Text*		tail;			// Pointer to end of text list
	size_t	size;			// Size of text in list

protected:
	Text* 	add(char*);
	Text*		search(char*) const;

	static uword	hash(char*);
};

struct StrList {
	// Node for a linked list of strings.

	StrList() : next(0), str(0) {}

	StrList*	next;
	char*		str;
};

extern TextList	text;

#endif
