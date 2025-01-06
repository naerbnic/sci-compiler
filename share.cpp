//	share.cpp	sc
// 	routines for locking/unlocking the class database so as to make sc
// 	network-compatible

#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "error.hpp"
#include "jeff.hpp"
#include "sc.hpp"
#include "sol.hpp"

bool abortIfLocked;
bool dontLock;

const int LOCKMODE = O_CREAT | O_EXCL | O_RDONLY;

static int haveLock;
static char lockFile[] = "$$$sc.lck";

static void Abort(int sig);

void Lock() {
  int fd;
  time_t now;
  time_t then;

  if (dontLock) return;

  // First trap interrupts so that if we're aborted while holding the lock
  // we can release it.
  signal(SIGINT, Abort);
  signal(SIGABRT, Abort);

  // Create the semaphore file.  If we can't do so, loop until we can.
  if ((fd = open(lockFile, LOCKMODE, (int)S_IREAD)) == -1) {
    if (abortIfLocked) Panic("Access to database denied");
    fprintf(stderr, "Waiting for access to class database");
    time(&then);
    while ((fd = open(lockFile, LOCKMODE, S_IREAD)) == -1) {
      do time(&now);
      while (now <= then);
      then = now;
      putc('.', stderr);
    }
    putc('\n', stderr);
  }

  close(fd);
  output("Class database locked.\n");
  haveLock = True;
}

void Unlock() {
  if (haveLock) {
    chmod(lockFile, (int)(S_IREAD | S_IWRITE));
    unlink(lockFile);
    output("Class database unlocked.\n");
  }
}

static void Abort(int) {
  Unlock();
  exit(1);
}
