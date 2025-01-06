//	share.hpp

#if !defined(LOCK_HPP)
#define LOCK_HPP

void	Lock();
void	Unlock();

extern Bool	abortIfLocked;
extern Bool	dontLock;

#endif
