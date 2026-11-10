// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>

#include "filesystem.h"
namespace foliq {
// A process lifetime OS lock. Never unlink lock files: that would split the lock inode.
class FileLock {
public:
    explicit FileLock(const fs::path& path);
    ~FileLock();
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

private:
#ifdef _WIN32
    void* handle;
#else
    int handle;
#endif
};
std::string read(const fs::path& path);
std::string fingerprint(const fs::path& path);
// Same-directory durable replacement. The previous complete revision remains in .bak.
void atomicWrite(const fs::path& path, const std::string& bytes);
// Optimistic conflict detection for documents opened outside the catalogue as well.
void watchDocument(const fs::path& path);
void checkDocument(const fs::path& path);
void acceptDocument(const fs::path& path);
}  // namespace foliq
