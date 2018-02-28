// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An Env is an interface used by the leveldb implementation to access
// operating system functionality like the filesystem etc.  Callers
// may wish to provide a custom Env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
//
// All Env implementations are safe for concurrent access from
// multiple threads without any external synchronization.

#ifndef QRPC_UTIL_FS_H
#define QRPC_UTIL_FS_H

#include <string>
#include <vector>
#include <stdarg.h>
#include <stdint.h>

namespace qrpc {

class FileLock;
class Slice;
class RandomAccessFile;
class SequentialFile;
class WritableFile;

class FileSystem {
public:
    /*
     * Helper to limit mmap file usage so that we do not end up running out
     * virtual memory or running into kernel perfomance problem for very
     * large databases.
     *
     * On success, return true.
     * On failure, return false.
     *
     * The default value is 1000 on x64, and 0 on i386.
     * The routine should only be accessed just once at the first time.
     */
    static bool InitMmapLimit(int num);

    /*
     * Create a brand new sequentially-readable file with the specified name.
     *
     * On success, return a pointer to the new file.
     * On failure, return NULL.
     *
     * The returned file will only be accessed by one thread at a time.
     */
    static SequentialFile* NewSequentialFile(const std::string &fname);

    /*
     * Create a brand new random access read-only file with the specified name.
     *
     * On success, return a pointer to the new file.
     * On failure, return NULL.
     *
     * The returned file may be concurrently accessed by multiple threads.
     */
    static RandomAccessFile* NewRandomAccessFile(const std::string &fname);

    /*
     * Create an object that writes to a new file with the specified name.
     * Deletes any existing file with the same name and creates a
     * new file.
     *
     * On success, return a pointer to the new file.
     * On failure, return NULL.
     *
     * The returned file will only be accessed by one thread at a time.
     */
    static WritableFile* NewWritableFile(const std::string &fname);

    /*
     * Reuse an existing file by opening it as writable, or create a new
     * file with the specified name.
     *
     * On success, return a pointer to the new file.
     * On failure, return NULL.
     *
     * The returned file will only be accessed by one thread at a time.
     */
    static WritableFile* NewReuseWritableFile(const std::string &name);

    /*
     * Returns true if the named file exists.
     */
    static bool FileExists(const std::string &fname);

    /*
     * Returns true iff the directory exists and is a directory.
     */
    static bool DirExists(const std::string &dname);

    /*
     * Store in *result the names of the children of the specified directory.
     * The names are relative to "dir".
     *
     * On success, return true.
     * On failure, return false.
     *
     * Original contents of *results are dropped and skip over './..' files.
     */
    static bool GetChildren(const std::string &dir, std::vector<std::string> *res);

    /*
     * Delete the named file.
     *
     * On success, return true.
     * On failure, return false.
     */
    static bool DeleteFile(const std::string &fname);

    /*
     * Create the specified directory.
     *
     * On success, return true.
     * On failure, return false.
     */
    static bool CreateDir(const std::string &dirname);

    /*
     * Delete the specified directory.
     *
     * On success, return true.
     * On failure, return false.
     */
    static bool DeleteDir(const std::string &dirname);

    /*
     * Store the size of fname in *file_size.
     *
     * On success, return true.
     * On failure, return false.
     */
    static bool GetFileSize(const std::string &fname, uint64_t *fsize);

    /*
     * Rename file src to target.
     *
     * On success, return true.
     * On failure, return false.
     */
    static bool RenameFile(const std::string &src, const std::string &target);
    
    /*
     * Lock the specified file.  Used to prevent concurrent access to
     * the same db by multiple processes.
     *
     * On success, return a pointer to the object that represents the
     * acquired lock.
     * On failure, return NULL.
     *
     * The caller should call UnlockFile() to release the lock.
     * If the process exits, the lock will be automatically released.
     *
     * If somebody else already holds the lock, finishes immediately
     * with a failure.  I.e., this call does not wait for existing locks
     * to go away.
     *
     * May create the named file if it does not already exist.
     */
    static FileLock* LockFile(const std::string &fname);

    /*
     * Release the lock acquired by a previous successful call to LockFile.
     * If 'lock' is NULL, nothing will be done and return true.
     *
     * On success, return true.
     * On failure, return false.
     *
     * REQUIRES: lock was returned by a successful LockFile() call
     * REQUIRES: lock has not already been unlocked.
     */
    static bool UnlockFile(FileLock *lock);
};

// A file abstraction for reading sequentially through a file
class SequentialFile {
public:
    SequentialFile() { }
    virtual ~SequentialFile();
    
    /*
     * Read up to "n" bytes from the file.  "scratch[0..n-1]" may be
     * written by this routine.  Sets "*result" to the data that was
     * read (including if fewer than "n" bytes were successfully read).
     * May set "*result" to point at data in "scratch[0..n-1]", so
     * "scratch[0..n-1]" must be live when "*result" is used.
     *
     * On success, return true.
     * On failure, return false.
     *
     * REQUIRES: External synchronization
     */
    virtual bool Read(size_t n, Slice *result, char *scratch) = 0;

    /*
     * Skip "n" bytes from the file. This is guaranteed to be no
     * slower that reading the same data, but may be faster.
     *
     * If end of file is reached, skipping will stop at the end of the
     * file, and Skip will return OK.
     *
     * On success, return true.
     * On failure, return false.
     *
     * REQUIRES: External synchronization
     */
    virtual bool Skip(uint64_t n) = 0;

private:
    SequentialFile(const SequentialFile &);
    void operator=(const SequentialFile &);
};

// A file abstraction for randomly reading the contents of a file.
class RandomAccessFile {
public:
    RandomAccessFile() { }
    virtual ~RandomAccessFile();

    /*
     * Read up to "n" bytes from the file starting at "offset".
     * "scratch[0..n-1]" may be written by this routine.  Sets "*result"
     * to the data that was read (including if fewer than "n" bytes were
     * successfully read).  May set "*result" to point at data in
     * "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
     * "*result" is used.  If an error was encountered, returns a non-OK
     * status.
     *
     * Safe for concurrent use by multiple threads.
     */
    virtual bool Read(uint64_t offset, size_t n,
                      Slice *result, char *scratch) const = 0;

private:
    // No copying allowed
    RandomAccessFile(const RandomAccessFile &);
    void operator=(const RandomAccessFile &);
};

/*
 * A file abstraction for sequential writing.  The implementation
 * must provide buffering since callers may append small fragments
 * at a time to the file.
 */
class WritableFile {
public:
    WritableFile() { }
    virtual ~WritableFile();

    virtual bool Append(const Slice &data) = 0;
    virtual bool Close() = 0;
    virtual bool Flush() = 0;
    virtual bool Sync() = 0;
    virtual bool Skip(uint64_t n) = 0;
    virtual bool GetFileSize(uint64_t *fsize) = 0;
    virtual bool Allocate(uint64_t offset, uint64_t len) = 0;

private:
    // No copying allowed
    WritableFile(const WritableFile &);
    void operator=(const WritableFile &);
};

// Identifies a locked file.
class FileLock {
public:
    FileLock() { }
    virtual ~FileLock();

private:
    // No copying allowed
    FileLock(const FileLock &);
    void operator=(const FileLock &);
};

// A utility routine: read contents of named file into *data
extern bool ReadFileToString(const std::string &fname, std::string *data);

// A utility routine: write "data" to the named file.
extern bool WriteStringToFile(const Slice &data, const std::string &fname);
extern bool WriteStringToFileSync(const Slice &data, const std::string &fname);

} // namespace qrpc

#endif /* QRPC_UTIL_FS_H */

