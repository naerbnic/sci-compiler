//	share.cpp	sc
// 	routines for locking/unlocking the class database so as to make sc
// 	network-compatible

#include <fcntl.h>
#include <sys/stat.h>

#include <cstdio>
#include <ctime>
#include <memory>
#include <stdexcept>

#include "scic/config.hpp"
#include "scic/error.hpp"
#include "util/platform/platform.hpp"

static int haveLock;
static char lockFile[] = "$$$sc.lck";

static std::unique_ptr<FileLock> fileLock = nullptr;

void Lock() {
  if (!fileLock) {
    fileLock = FileLock::Create(lockFile);
  }
  time_t now;
  time_t then;

  if (gConfig->dontLock) return;

  // Create the semaphore file.  If we can't do so, loop until we can.
  if (!fileLock->LockFile()) {
    if (gConfig->abortIfLocked)
      throw std::runtime_error("Access to database denied");
    fprintf(stderr, "Waiting for access to class database");
    time(&then);
    while (!fileLock->LockFile()) {
      do time(&now);
      while (now <= then);
      then = now;
      putc('.', stderr);
    }
    putc('\n', stderr);
  }

  output("Class database locked.\n");
  haveLock = true;
}

void Unlock() {
  if (haveLock) {
    fileLock->ReleaseLock();
    output("Class database unlocked.\n");
  }
}
