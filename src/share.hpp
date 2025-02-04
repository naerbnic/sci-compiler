//	share.hpp

#if !defined(LOCK_HPP)
#define LOCK_HPP

void Lock();
void Unlock();

extern bool gAbortIfLocked;
extern bool gDontLock;

#endif
