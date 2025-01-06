//	share.hpp

#if !defined(LOCK_HPP)
#define LOCK_HPP

void Lock();
void Unlock();

extern bool abortIfLocked;
extern bool dontLock;

#endif
