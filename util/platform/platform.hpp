// Platform-specific definitions and functions.

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <cstdio>
#include <string_view>
#include <memory>
#include <string_view>

// A platform independent way of using advisory locks on a lock file. This
// should release the lock when the FileLock object is destroyed, or the
// process exits.
class FileLock {
 public:
  // Create a new FileLock for the given path. No lock is taken at this time.
  static std::unique_ptr<FileLock> Create(std::string_view path);
  virtual ~FileLock() = default;

  virtual bool LockFile() = 0;
  virtual void ReleaseLock() = 0;
};

bool IsTTY(FILE* fp);
void DeletePath(std::string_view path);
FILE* CreateOutputFile(std::string_view path);
bool FileExists(std::string_view path);

#endif  // PLATFORM_HPP