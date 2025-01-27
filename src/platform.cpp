#include "platform.hpp"

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__linux__)

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <cassert>
#include <string>

#ifdef __linux__
#include <sys/file.h>
#endif

#define OMODE (int)(O_CREAT | O_RDWR | O_TRUNC)
#define PMODE (int)(S_IREAD | S_IWRITE)

bool IsTTY(FILE* fp) { return isatty(fileno(fp)); }
void DeletePath(std::string_view path) { unlink(std::string(path).c_str()); }

FILE* CreateOutputFile(std::string_view path) {
  int fd = open(std::string(path).c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd == -1) return nullptr;
  return fdopen(fd, "w+");
}

class FileLockImpl : public FileLock {
 public:
  FileLockImpl(std::string path, int fd) : path_(path), fd_(fd) {}

  ~FileLockImpl() { close(fd_); }

  bool LockFile() override {
    if (flock(fd_, LOCK_EX | LOCK_NB) == -1) {
      assert(errno == EWOULDBLOCK);
      return false;
    }

    locked_ = true;
    return true;
  }

  void ReleaseLock() override { flock(fd_, LOCK_UN); }

 private:
  std::string path_;
  int fd_;
  bool locked_ = false;
};

std::unique_ptr<FileLock> FileLock::Create(std::string_view path) {
  int fd = open(std::string(path).c_str(), O_CREAT, 0600);
  if (fd == -1) return nullptr;
  return std::make_unique<FileLockImpl>(std::string(path), fd);
}

#elif defined(_WIN32)
#error "Windows is not yet implemented.
#else
#error "Platform is unsupported. Check the platform.hpp file to add support."
#endif