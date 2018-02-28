// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/falloc.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <deque>
#include <set>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/fs.h"
#include "src/qrpc/util/slice.h"

using namespace std;

namespace qrpc {

SequentialFile::~SequentialFile()
{
}

RandomAccessFile::~RandomAccessFile()
{
}

WritableFile::~WritableFile()
{
}

FileLock::~FileLock()
{
}

/*
 * Helper class to limit mmap file usage so that we do not end up
 * running out virtual memory or running into kernel performance
 * problems for very large databases.
 *
 * Up to 1000 mmaps for 64-bit binaries; none for smaller pointer sizes.
 */
class MmapLimiter {
public:
    explicit MmapLimiter();
    ~MmapLimiter();
    
    // If another mmap slot is available, acquire it and return true. Else return false.
    bool Acquire();

    // Release a slot acquired by a previous call to Acquire() that returned true.
    void Release();

private:
    MmapLimiter(const MmapLimiter&);
    void operator=(const MmapLimiter&);

public:
    atomic_t allowed_;
    pthread_mutex_t lock_;
};

MmapLimiter::MmapLimiter()
{
    int limit = (sizeof(void *) >= 8 ? 1000 : 0);
    atomic_set(&allowed_, limit);
    pthread_mutex_init(&lock_, NULL);
}

MmapLimiter::~MmapLimiter()
{

}

inline bool MmapLimiter::Acquire()
{
    bool rc = true;

    if (atomic_read(&allowed_) <= 0) {
        return false;
    }

    pthread_mutex_lock(&lock_);
    if (unlikely(atomic_read(&allowed_) <= 0)) {
        pthread_mutex_unlock(&lock_);
        rc = false;
        goto out;
    }
    atomic_dec(&allowed_);
    pthread_mutex_unlock(&lock_);

out:
    return rc;
}

inline void MmapLimiter::Release()
{
    atomic_inc(&allowed_);
}

static int LockOrUnlock(int fd, bool lock)
{
    errno = 0;

    struct flock f;
    memset(&f, 0, sizeof(f));
    f.l_type    = (lock ? F_WRLCK : F_UNLCK);
    f.l_whence  = SEEK_SET;
    f.l_start   = 0;
    f.l_len     = 0; // Lock/unlock entire file

    return fcntl(fd, F_SETLK, &f);
}

class PosixFileLock : public FileLock {
public:
    int fd_;
    std::string name_;
};

/*
 * Set of locked files.  We keep a separate set instead of just
 * relying on fcntrl(F_SETLK) since fcntl(F_SETLK) does not provide
 * any protection against multiple uses from the same process.
 */
class PosixLockTable {
public:
    PosixLockTable();
    ~PosixLockTable();

    bool Insert(const std::string &fname);
    void Remove(const std::string &fname);

private:
    pthread_mutex_t mu_;
    std::set<std::string> locked_files_;
};

PosixLockTable::PosixLockTable()
{
    pthread_mutex_init(&mu_, NULL);
}

PosixLockTable::~PosixLockTable()
{

}

inline bool PosixLockTable::Insert(const string &fname)
{
    bool rc;

    pthread_mutex_lock(&mu_);
    rc = locked_files_.insert(fname).second;
    pthread_mutex_unlock(&mu_);

    return rc;
}

inline void PosixLockTable::Remove(const string &fname)
{
    pthread_mutex_lock(&mu_);
    locked_files_.erase(fname);
    pthread_mutex_unlock(&mu_);
}

namespace {

PosixLockTable locks;
MmapLimiter mmap_limit;

} // anonymous namespace

class PosixSequentialFile: public SequentialFile {
public:
    PosixSequentialFile(const std::string &fname, FILE *f);
    virtual ~PosixSequentialFile();
    
    virtual bool Read(size_t n, Slice *result, char *scratch);
    virtual bool Skip(uint64_t n);

private:
    FILE* file_;
    std::string filename_;
};

PosixSequentialFile::PosixSequentialFile(const std::string& fname, FILE* f)
    : file_(f), filename_(fname)
{

}

PosixSequentialFile::~PosixSequentialFile()
{
    fclose(file_);
}

bool PosixSequentialFile::Read(size_t n, Slice *result, char *scratch)
{
    bool rc = true;
    size_t r = fread(scratch, 1, n, file_);
    if (r < n) {
        if (feof(file_)) {
            // We leave status as ok if we hit the end of the file
        } else {
            // A partial read with an error: return a non-ok status
            rc = false;
            LOG(ERROR) << "read: " << filename_ << " failed, ec: " << errno;
        }
    }
    *result = Slice(scratch, r);

    return rc;
}

bool PosixSequentialFile::Skip(uint64_t n)
{
    if (fseek(file_, n, SEEK_CUR)) {
      LOG(ERROR) << "seek: " << filename_ << " failed, ec: " << errno;
      return false;
    }
    return true;
}

// pread() based random-access
class PosixRandomAccessFile: public RandomAccessFile {
public:
    PosixRandomAccessFile(const std::string &fname, int fd);
    virtual ~PosixRandomAccessFile();
    
    virtual bool Read(uint64_t offset, size_t n, Slice *result, char *scratch) const;

private:
    int fd_;
    std::string filename_;
};

PosixRandomAccessFile::PosixRandomAccessFile(const string &fname, int fd)
    : fd_(fd), filename_(fname)
{

}

PosixRandomAccessFile::~PosixRandomAccessFile()
{

}

bool PosixRandomAccessFile::Read(uint64_t offset, size_t n,
                                 Slice *result, char *scratch) const
{
    bool rc = true;
    ssize_t r = pread(fd_, scratch, n, static_cast<off_t>(offset));
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
        // An error: return a non-ok status
        rc = false;
        LOG(ERROR) << "read: " << filename_ << " failed, ec: " << errno;
    }
    return rc;
}

// mmap() based random-access
class PosixMmapReadableFile: public RandomAccessFile {
public:
    // base[0,length-1] contains the mmapped contents of the file.
    PosixMmapReadableFile(const std::string &fname, void *base,
                          size_t length, MmapLimiter *limiter);
    virtual ~PosixMmapReadableFile();

    virtual bool Read(uint64_t offset, size_t n,
            Slice* result, char* scratch) const;

private:
    std::string filename_;
    void* mmapped_region_;
    size_t length_;
    MmapLimiter* limiter_;
};

PosixMmapReadableFile::PosixMmapReadableFile(const string &fname, void *base,
                                             size_t length, MmapLimiter *limiter)
    : filename_(fname), mmapped_region_(base)
    , length_(length), limiter_(limiter)
{

}

PosixMmapReadableFile::~PosixMmapReadableFile()
{
    munmap(mmapped_region_, length_);
    limiter_->Release();
}

bool PosixMmapReadableFile::Read(uint64_t offset, size_t n,
                                 Slice *result, char *scratch) const
{
    bool rc = true;
    if (offset + n > length_) {
        *result = Slice();
        rc = false;
        LOG(ERROR) << "read: " << filename_ << " failed, ec: EINVAL";
    } else {
        *result = Slice(reinterpret_cast<char*>(mmapped_region_) + offset, n);
    }
    return rc;
}

class PosixWritableFile : public WritableFile {
public:
    PosixWritableFile(const std::string &fname, FILE *f);
    virtual ~PosixWritableFile();
    
    virtual bool Append(const Slice &data);
    virtual bool Close();
    virtual bool Flush();
    virtual bool Sync();
    virtual bool Skip(uint64_t n);
    virtual bool GetFileSize(uint64_t *fsize);
    virtual bool Allocate(uint64_t offset, uint64_t len);

private:
    bool SyncDirIfManifest();

private:
    FILE* file_;
    uint64_t fsize_;
    std::string filename_;
};

PosixWritableFile::PosixWritableFile(const string &fname, FILE *f)
    : file_(f), fsize_(0), filename_(fname)
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) != 0) {
        LOG(FATAL) << "fstat: " << fname << " failed, ec: " << errno;
    } else {
        fsize_ = sbuf.st_size;
    }
}

PosixWritableFile::~PosixWritableFile()
{
    if (file_ != NULL) {
        // Ignoring any potential errors
        fclose(file_);
    }
}

bool PosixWritableFile::Append(const Slice &data)
{
    bool rc = true;
    size_t r = fwrite(data.data(), 1, data.size(), file_);
    if (r != data.size()) {
        rc = false;
        LOG(ERROR) << "write: " << filename_ << " failed, ec: " << errno;
    }
    fsize_ += data.size();
    return rc;
}

bool PosixWritableFile::Close()
{
    bool rc = true;
    if (fclose(file_) != 0) {
        rc = false;
        LOG(ERROR) << "close: " << filename_ << " failed, ec: " << errno;
    }
    file_ = NULL;
    return rc;
}

bool PosixWritableFile::Flush()
{
    if (fflush_unlocked(file_) != 0) {
        LOG(ERROR) << "flush: " << filename_ << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool PosixWritableFile::SyncDirIfManifest()
{
    const char* f = filename_.c_str();
    const char* sep = strrchr(f, '/');
    Slice basename;
    std::string dir;

    if (sep == NULL) {
        dir = ".";
        basename = f;
    } else {
        dir = std::string(f, sep - f);
        basename = sep + 1;
    }

    bool rc = true;
    if (basename.starts_with("MANIFEST")) {
        int fd = open(dir.c_str(), O_RDONLY);
        if (fd < 0) {
            rc = false;
            LOG(ERROR) << "open dir: " << dir << " failed, ec: " << errno;
        } else {
            if (fsync(fd) < 0) {
                rc = false;
                LOG(ERROR) << "fsync dir: " << dir << " failed, ec: " << errno;
            }
            close(fd);
        }
    }
    return rc;
}

bool PosixWritableFile::Sync()
{
    // Ensure new files referred to by the manifest are in the filesystem.
    if (!SyncDirIfManifest()) {
        return false;
    }

    if (fflush(file_) != 0 || fdatasync(fileno(file_)) != 0) {
        LOG(ERROR) << "fdatasync: " << filename_ << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool PosixWritableFile::Skip(uint64_t n)
{
    if (fseek(file_, n, SEEK_SET)) {
      LOG(ERROR) << "seek: " << filename_ << " failed, ec: " << errno;
      return false;
    }
    return true;
}

bool PosixWritableFile::GetFileSize(uint64_t *fsize)
{
    *fsize = fsize_;
    return true;
}

bool PosixWritableFile::Allocate(uint64_t offset, uint64_t len)
{
    int fd = fileno(file_);
    if (fd == -1) {
        LOG(ERROR) << "fileno: " << filename_ << " failed, ec: " << errno;
        return false;
    }

    //if (posix_fallocate(fd, offset, len)) {
    //    LOG(ERROR) << "fallocate: " << filename_ << " failed, ec: " << errno;
    //    return false;
    //}
    if (fallocate(fd, FALLOC_FL_KEEP_SIZE, offset, len)) {
        LOG(ERROR) << "fallocate: " << filename_ << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool FileSystem::InitMmapLimit(int num)
{
    if (num < 0) {
        return false;
    }

    int init = (sizeof(void *) >= 8 ? 1000 : 0);
    if (atomic_read(&mmap_limit.allowed_) != init) {
        return false;
    }
    atomic_set(&mmap_limit.allowed_, num);

    return true;
}

SequentialFile* FileSystem::NewSequentialFile(const string &fname)
{
    SequentialFile *rc = NULL;

    FILE* f = fopen(fname.c_str(), "r");
    if (f == NULL) {
        LOG(ERROR) << "open: " << fname << " failed, ec: " << errno;
    } else {
        rc = new PosixSequentialFile(fname, f);
    }
    return rc;
}

RandomAccessFile* FileSystem::NewRandomAccessFile(const std::string &fname)
{
    RandomAccessFile *rc = NULL;

    int fd = open(fname.c_str(), O_RDONLY);

    if (fd < 0) {
        LOG(ERROR) << "open: " << fname << " failed, ec: " << errno;
    } else if (mmap_limit.Acquire()) {
        uint64_t size;

        bool res = GetFileSize(fname, &size);
        if (res) {
            void* base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
            if (base != MAP_FAILED) {
                rc = new PosixMmapReadableFile(fname, base, size, &mmap_limit);
            } else {
                res = false;
                LOG(ERROR) << "mmap: " << fname << " failed, ec: " << errno;
            }
        }
        close(fd);
        if (!res) {
            mmap_limit.Release();
        }
    } else {
        rc = new PosixRandomAccessFile(fname, fd);
    }
    return rc;
}

WritableFile* FileSystem::NewWritableFile(const std::string &fname)
{
    WritableFile *rc = NULL;

    FILE* f = fopen(fname.c_str(), "w");
    if (f == NULL) {
        LOG(ERROR) << "open: " << fname << " failed, ec: " << errno;
    } else {
        rc = new PosixWritableFile(fname, f);
    }
    return rc;
}

WritableFile* FileSystem::NewReuseWritableFile(const std::string &fname)
{
    WritableFile *rc = NULL;
    FILE *f = NULL;

    if (FileExists(fname)) {
        f = fopen(fname.c_str(), "a");
    } else {
        f = fopen(fname.c_str(), "w");
    }
    if (f == NULL) {
        LOG(ERROR) << "open: " << fname << " failed, ec: " << errno;
    } else {
        rc = new PosixWritableFile(fname, f);
    }
    return rc;
}

bool FileSystem::FileExists(const string &fname)
{
    return access(fname.c_str(), F_OK) == 0;
}

bool FileSystem::DirExists(const string &dname)
{
    struct stat statbuf;

    if (stat(dname.c_str(), &statbuf) == 0) {
        return S_ISDIR(statbuf.st_mode);
    }

    return false; /* stat() failed return false */
}

bool FileSystem::GetChildren(const string &dir, vector<string> *result)
{
    result->clear();

    DIR* d = opendir(dir.c_str());
    if (d == NULL) {
        LOG(ERROR) << "opendir: " << dir << " failed, ec: " << errno;
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (!strncmp(entry->d_name, "..", strlen(entry->d_name))) {
            continue;
        }
        result->push_back(entry->d_name);
    }
    closedir(d);

    return true;
}

bool FileSystem::DeleteFile(const std::string &fname)
{
    if (unlink(fname.c_str()) != 0) {
        LOG(ERROR) << "delete: " << fname << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool FileSystem::CreateDir(const std::string &name)
{
    if (mkdir(name.c_str(), 0755) != 0) {
        LOG(ERROR) << "create dir: " << name << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool FileSystem::DeleteDir(const std::string &name)
{
    if (rmdir(name.c_str()) != 0) {
        LOG(ERROR) << "delete dir: " << name << " failed, ec: " << errno;
        return false;
    }
    return true;
}

bool FileSystem::GetFileSize(const std::string &fname, uint64_t *size)
{
    struct stat sbuf;

    if (stat(fname.c_str(), &sbuf) != 0) {
        *size = 0;
        LOG(ERROR) << "stat: " << fname << " failed, ec: " << errno;
        return false;
    } else {
        *size = sbuf.st_size;
        return true;
    }
}

bool FileSystem::RenameFile(const std::string &src, const std::string &target)
{
    if (rename(src.c_str(), target.c_str()) != 0) {
        LOG(ERROR) << "rename: " << src << " failed, ec: " << errno;
        return false;
    }
    return true;
}

FileLock* FileSystem::LockFile(const std::string &fname)
{
    FileLock *lock = NULL;

    int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        LOG(ERROR) << "open: " << fname << " failed, ec: " << errno;
    } else if (!locks.Insert(fname)) {
        close(fd);
        LOG(WARNING) << fname << " already held by process";
    } else if (LockOrUnlock(fd, true) == -1) {
        close(fd);
        locks.Remove(fname);
        LOG(ERROR) << "lock: " << fname << " failed, ec: " << errno;
    } else {
        PosixFileLock* my_lock = new PosixFileLock;
        my_lock->fd_ = fd;
        my_lock->name_ = fname;

        lock = my_lock;
    }

    return lock;
}

bool FileSystem::UnlockFile(FileLock *lock)
{
    if (unlikely(!lock)) {
        return true;
    }

    PosixFileLock* my_lock = reinterpret_cast<PosixFileLock*>(lock);
    bool rc = true;

    if (LockOrUnlock(my_lock->fd_, false) == -1) {
        rc = false;
        LOG(ERROR) << "unlock: " << my_lock->name_ << " failed, ec: " << errno;
    }

    locks.Remove(my_lock->name_);
    close(my_lock->fd_);
    delete my_lock;

    return rc;
}

static bool DoWriteStringToFile(const Slice& data,
                                const std::string& fname, bool should_sync)
{
    WritableFile* file;
    
    file = FileSystem::NewWritableFile(fname);
    if (!file) {
        return false;
    }

    bool rc = file->Append(data);
    if (rc && should_sync) {
        rc = file->Sync();
    }
    if (rc) {
        rc = file->Close();
    }
    delete file; // Will auto-close if we did not close above
    if (!rc) {
        FileSystem::DeleteFile(fname);
    }
    return rc;
}

bool WriteStringToFile(const Slice &data, const std::string &fname)
{
    return DoWriteStringToFile(data, fname, false);
}

bool WriteStringToFileSync(const Slice &data, const std::string &fname)
{
    return DoWriteStringToFile(data, fname, true);
}

bool ReadFileToString(const std::string &fname, std::string *data)
{
    data->clear();

    SequentialFile* file;

    file = FileSystem::NewSequentialFile(fname);
    if (!file) {
        return false;
    }

    static const int kBufferSize = 8192;
    char* space = new char[kBufferSize];
    bool rc = true;

    while (true) {
        Slice fragment;

        rc = file->Read(kBufferSize, &fragment, space);
        if (!rc) {
            break;
        }
        data->append(fragment.data(), fragment.size());
        if (fragment.empty()) {
            break;
        }
    }
    delete[] space;
    delete file;

    return rc;
}

} // namespace qrpc
