//	mem.cpp
// 	memory management routines

#include	<dos.h>
#include	<stdio.h>

#include "sol.hpp"

#include	"sc.hpp"

#include	"error.hpp"
#include	"mem.hpp"

Bool	writeMemSizes;

//	if we're running under Windows, trust it to manage our memory
#if !defined(WINDOWS) && !defined(__386__)

#include "emsblock.hpp"

class	MemBlk;

struct MemList
{
	MemList(size_t s, uint n);
	virtual ~MemList() {}

	void*	operator new(size_t size);
	void	operator delete(void* ptr);

	virtual void*	get();
	virtual Bool	put(void* mem);

	void		display();
	Bool		check();
	uint		blksFree();
	uint		numAllocated();

	MemBlk*	head;
	size_t	size;				// size of memory allocation units
	uint		numBlks;			// number of memory allocation units per MemBlk
};

struct EMSMemList : MemList
{
	EMSMemList(size_t s);
	~EMSMemList();

	void*	get();

	static EMSBlock* ems;
};

EMSBlock* EMSMemList::ems;

struct MemBlk
{
	MemBlk(MemList* bp, size_t s, uint n);
	MemBlk();
	void		init(MemList* bp, size_t s, uint n);

	virtual ~MemBlk();

	void*	operator new(size_t s);
	void	operator delete(void*);

	virtual Bool	put(MemList* list, void* mem);

	void*	get();
	void	display();
	Bool	check();

	MemBlk*	next;
	MemBlk*	prev;
	char*		start;
	char*		end;
	char*		head;
	uint		numFree;
};

struct EMSMemBlk : MemBlk
{
	EMSMemBlk(EMSMemList* bp, size_t s, uint n);
	~EMSMemBlk();

	Bool	put(MemList* list, void* mem);
};

#define	Normalize(p)	\
				(char*) MK_FP(FP_SEG(p) + (FP_OFF(p) >> 4), FP_OFF(p) & 0x0f)

#define	NullP(p)			!((FP_SEG(p) | FP_OFF(p)))

#define	LessP(p1, p2)																\
				(FP_SEG(p1) < FP_SEG(p2) ||										\
				(FP_SEG(p1) == FP_SEG(p2) && FP_OFF(p1) < FP_OFF(p2)))
#define	LessEqP(p1, p2)															\
				(FP_SEG(p1) <= FP_SEG(p2) ||										\
				(FP_SEG(p1) == FP_SEG(p2) && FP_OFF(p1) <= FP_OFF(p2)))

// memList is the array of lists of blocks of fixed size memory units from
// which small allocations are made.  The MemLists in this array MUST be
// ordered in increasing size of the memory units contained in the blocks.
class MemListList {
public:
	enum { nLists = 7};

	Bool	active;

	MemListList()
	{
		lists[0] = new MemList( 8, 128);
		lists[1] = new MemList(14, 256);
		lists[2] = new MemList(16, 512);
		lists[3] = new EMSMemList(18);
		lists[4] = new MemList(18, 256);
		lists[5] = new MemList(22, 256);
		lists[6] = new MemList(24, 128);
		active = True;
	}

	~MemListList()
	{
		for (int i = 0; i < MemListList::nLists; i++)
			delete lists[i];
		active = False;
	}

	MemList* operator[](int i)
	{
		return lists[i];
	}

protected:
	MemList*	lists[MemListList::nLists];

} static lists;

static ulong	memSizes[32];
static ulong	newCalls;

static void	WriteMemSizes();
static void	MemDisplay();

///////////////////////////////////////////////////
// Memory functions
///////////////////////////////////////////////////

void*
operator new(
	size_t n)
{
	// Error checking malloc() call.

	++newCalls;
	if (writeMemSizes && n < 32)
		++memSizes[n];

	void*	mp = 0;

	// Search the fixed size memory lists to see if the requested size
	// should be allocated from one of them.
	for (int i = 0 ; lists.active && lists[i] && !mp && i < MemListList::nLists; ++i)
		if (n <= lists[i]->size)
			mp = lists[i]->get();

	// If the memory was not available from the fixed sized lists, use malloc.
	if (!mp)
		mp = malloc(n);

	if (!mp) {
		WriteMemSizes();
		Fatal("Out of memory. Attempted to allocate %u bytes.", n);
	}

	return mp;
}

void
operator delete(
	void* mp)
{
	if (!mp || !lists.active)
		return;

	// See if this memory block should be returned to one of the
	// memory lists.
	for (int i = 0; i < MemListList::nLists && !lists[i]->put(mp); ++i)
		;

	// If not returned to a list, just free it.
	if (i >= MemListList::nLists)
		free(mp);
}

///////////////////////////////////////////////////
// Class MemList
///////////////////////////////////////////////////

MemList::MemList(
	size_t	s,
	uint		n) :

	size(s), numBlks(n), head(0)
{
}

void*
MemList::operator new(
	size_t	size)
{
	return malloc(size);
}

void
MemList::operator delete(
	void*		ptr)
{
	if (ptr)
		free(ptr);
}

void*
MemList::get()
{
	void*	mem;
	for (MemBlk* mp = head; mp; mp = mp->next)
		if (mem = mp->get())
			return mem;

	new MemBlk(this, size, numBlks);
	return get();
}

Bool
MemList::put(void* p)
{
	for (MemBlk* mp = head; mp; mp = mp->next)
		if (mp->put(this, p))
			return True;

	return False;
}

void
MemList::display()
{
	printf("MemList of %dx%d byte MemBlks\n", numBlks, size);
	for (MemBlk* mp = head; !NullP(mp); mp = mp->next)
		mp->display();
}

Bool
MemList::check()
{
	for (MemBlk* mp = head; !NullP(mp); mp = mp->next)
		if (mp->numFree > numBlks || !mp->check())
			return False;

	return True;
}

uint
MemList::blksFree()
{
	uint	numFree = 0;
	for (MemBlk* mp = head; !NullP(mp); mp = mp->next)
		numFree += mp->numFree;

	return numFree;
}

uint
MemList::numAllocated()
{
	uint	num = 0;
	for (MemBlk* mp = head; !NullP(mp); mp = mp->next)
		++num;

	return num;
}

///////////////////////////////////////////////////
// Class MemBlk
///////////////////////////////////////////////////

MemBlk::MemBlk() : start(0), end(0), next(0), prev(0), head(0), numFree(0)
{
}

MemBlk::MemBlk(
	MemList* bp,
	size_t	s,
	uint		n) :
	
	start(0), end(0), next(0), prev(0), head(0), numFree(0)
{
	head = New char[s * n];
	init(bp, s, n);
}

void
MemBlk::init(
	MemList*	bp,
	size_t	s,
	uint		n)
{
	assert(bp);

	head = Normalize(head);
	start = head;
	void* blk = head + s * n;
	end = Normalize(blk);
	numFree = n;

	// Link this into the MemList.
	next = bp->head;
	prev = 0;
	if (next)
		next->prev = this;
	bp->head = this;

	// Initialize the free list of this MemBlk.
	char* mp;
	char* np;
	for (mp = head, np = Normalize(mp + s) ;
		  LessP(np, end) ;
		  mp = np, np = Normalize(mp + s))
		*(void** ) mp = np;

	*(void**) mp = 0;
}

void*
MemBlk::operator new(
	size_t s)
{
	void* mem = malloc(s);
	if (!mem)
		Fatal("Out of memory. Attempted to allocate %u bytes.", s);
	return mem;
}

void
MemBlk::operator delete(
	void*	ptr)
{
	if (ptr)
		free(ptr);
}

MemBlk::~MemBlk()
{
	free(start);
}

void*
MemBlk::get()
{
	void* mp = head;				// get pointer to head of free list
	if (!NullP(mp)) {				// if there is free memory in this block,
		head = *(char**) mp;	// ... remove this memory from free list
		--numFree;
	}

	return mp;
}

Bool
MemBlk::put(
	MemList*	list,
	void*		mp)
{
	if (LessEqP(start, mp) && LessP(mp, end)) {
		*(void**) mp = head;
		head = (char*) mp;

		// If this block is entirely free, return it to malloc()'s free pool.
		++numFree;
		if (numFree == list->numBlks) {
			// Unlink from the MemList.
			if (list->head == this)
				list->head = next;
			if (prev)
				prev->next = next;
			if (next)
				next->prev = prev;

			// Delete myself
			delete this;
		}
		return True;
	}

	return False;
}

void
MemBlk::display()
{
	printf("\tMemBlk from %p to %p, %d free blocks\n", start, end, numFree);
}

Bool
MemBlk::check()
{
	return FP_SEG(start) < FP_SEG(end);
}

///////////////////////////////////////////////////
// Class EMSMemList
///////////////////////////////////////////////////

EMSMemList::EMSMemList(
	size_t	s) :

	MemList(s, 0)
{
	//	preallocate the EMS block

	ems = new EMSBlock;
	if (ems && (numBlks = ems->size / s))
		new EMSMemBlk(this, size, numBlks);
}

EMSMemList::~EMSMemList()
{
	delete ems;
}

void*
EMSMemList::get()
{
	//	don't go back for more if the first allocation runs out

	return head ? head->get() : 0;
}

///////////////////////////////////////////////////
// Class EMSMemBlk
///////////////////////////////////////////////////

EMSMemBlk::EMSMemBlk(
	EMSMemList*	bp,
	size_t		s,
	uint			n)
{
	//	give us EMS as our memory, if there is any

	if (!(head = (char*) EMSMemList::ems->base))
		return;

	init(bp, s, n);
}

EMSMemBlk::~EMSMemBlk()
{
	//	don't let it be deleted, since it's EMS

	start = 0;
}

Bool
EMSMemBlk::put(
	MemList*,
	void*		mp)
{
	//	don't give back the memory when the block has been totally freed

	if (LessEqP(start, mp) && LessP(mp, end)) {
		*((void** ) mp) = head;
		head = (char*) mp;

		++numFree;
		return True;
	}

	return False;
}

///////////////////////////////////////////////////
// Auxiliary functions
///////////////////////////////////////////////////

void
WriteMemSizes()
{
	MemDisplay();
	printf("Memory blocks:\n");
	for (int i = 0; i < MemListList::nLists; ++i)
		lists[i]->display();
}

struct _HeapInfo {
	ulong	size;
	ulong	count;
	ulong	largest;
};

static ulong	HeapFree(int (*fp)(heapinfo* ), _HeapInfo* hp);

void
MemDisplay()
{
	_HeapInfo	hi;

	HeapFree(heapwalk, &hi);
	printf("Heap: %lu bytes free in %lu blocks.  Largest = %lu\n",
		hi.size, hi.count, hi.largest);
	ulong totalFree = hi.size;

	for (int i = 0; i < MemListList::nLists; ++i) {
		MemList* ml = lists[i];
		uint numAlloc = ml->numAllocated();
		ulong bytesFree = ml->blksFree()*  ml->size;
		ulong numBytes = ml->numBlks*  ml->size*  numAlloc;
		printf("\t%6lu bytes in %2d %3dx%2d byte blocks: %6lu bytes free\n",
			numBytes, numAlloc, ml->numBlks, ml->size, bytesFree);
		totalFree += bytesFree;
		}

	printf("Total free bytes: %lu\n", totalFree);
}

static ulong
HeapFree(
	int			(*fp)(heapinfo*),
	_HeapInfo*	hp)
{
	ulong size = fp == farheapwalk ? farcoreleft() : coreleft();

	if (hp) {
		hp->count = 1;
		hp->largest = size;
	}

	heapinfo	hi;
	hi.ptr = 0;
	while (fp(&hi) == _HEAPOK) {
		if (!hi.in_use) {
			size += hi.size;
			if (hp) {
				++hp->count;
				if (hi.size > hp->largest)
					hp->largest = hi.size;
			}
		}
	}

	if (hp)
		hp->size = size;

	return size;
}

#endif
