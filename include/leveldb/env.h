// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An Env is an interface used by the leveldb implementation to access
// operating system functionality like the filesystem etc.  Callers
// may wish to provide a custom Env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
// 
// Env 是 leveldb 实现用于跨平台访问操作系统功能（如文件系统等）的接口。当打开数据库
// 以获得良好的增益控制时，调用者可能希望提供自定义的 Env 对象; 例如限制文件系统操作
//
// All Env implementations are safe for concurrent access from
// multiple threads without any external synchronization.
// 
// 所有 Env 实现都可以安全地从多个线程并发访问，而无需任何外部同步

#ifndef STORAGE_LEVELDB_INCLUDE_ENV_H_
#define STORAGE_LEVELDB_INCLUDE_ENV_H_

#include <string>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include "leveldb/status.h"

namespace leveldb {

class FileLock;
class Logger;
class RandomAccessFile;
class SequentialFile;
class Slice;
class WritableFile;

class Env {
 public:
  Env() { }
  virtual ~Env();

  // Return a default environment suitable for the current operating
  // system.  Sophisticated users may wish to provide their own Env
  // implementation instead of relying on this default environment.
  //
  // The result of Default() belongs to leveldb and must never be deleted.
  // 
  // 返回适合当前操作系统的默认环境。复杂的用户可能希望提供自己的 Env 实现，而不是
  // 依赖这个默认环境
  // 
  // Default() 的结果属于 leveldb 进程的，绝不能删除
  static Env* Default();

  // Create a brand new sequentially-readable file with the specified name.
  // On success, stores a pointer to the new file in *result and returns OK.
  // On failure stores NULL in *result and returns non-OK.  If the file does
  // not exist, returns a non-OK status.
  //
  // The returned file will only be accessed by one thread at a time.
  // 
  // 使用指定的名称创建一个全新的顺序访问的可读文件。成功时，在 *result 中存储一个指向
  // 新文件的指针并返回 OK。失败时，在 *result 中存储 NULL 并返回 non-OK。如果文件不
  // 存在，则返回 non-OK
  // 
  // 返回的文件一次只能被一个线程访问（非线程安全）
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) = 0;

  // Create a brand new random access read-only file with the
  // specified name.  On success, stores a pointer to the new file in
  // *result and returns OK.  On failure stores NULL in *result and
  // returns non-OK.  If the file does not exist, returns a non-OK
  // status.
  //
  // The returned file may be concurrently accessed by multiple threads.
  // 
  // 使用指定的名称创建一个全新的随机访问的只读文件。成功时，在 *result 中存储一个指向
  // 新文件的指针并返回 OK。 失败时在 *result 中存储 NULL 并返回 non-OK。 如果文件
  // 不存在，则返回 non-OK
  // 
  // 返回的文件可能被多个线程同时访问（只读）
  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) = 0;

  // Create an object that writes to a new file with the specified
  // name.  Deletes any existing file with the same name and creates a
  // new file.  On success, stores a pointer to the new file in
  // *result and returns OK.  On failure stores NULL in *result and
  // returns non-OK.
  //
  // The returned file will only be accessed by one thread at a time.
  // 
  // 创建一个具有指定名称的可写入的新文件的对象。删除具有相同名称的任何现有文件
  // 并创建一个新文件。成功时，在 *result 中存储一个指向新文件的指针并返回 OK。
  // 失败时在 *result 中存储 NULL 并返回 non-OK
  // 
  // 返回的文件一次只能被一个线程访问（非线程安全）
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) = 0;

  // Returns true iff the named file exists.
  // 
  // 如果文件存在，返回 true
  virtual bool FileExists(const std::string& fname) = 0;

  // Store in *result the names of the children of the specified directory.
  // The names are relative to "dir".
  // Original contents of *results are dropped.
  // 
  // 在 *result 中存储指定目录下子目录名称，路径名称相对 "dir"。 *result 向量的原
  // 始内容被删除
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

  // Delete the named file.
  // 
  // 删除指定文件
  virtual Status DeleteFile(const std::string& fname) = 0;

  // Create the specified directory.
  // 
  // 创建指定目录
  virtual Status CreateDir(const std::string& dirname) = 0;

  // Delete the specified directory.
  // 
  // 删除指定目录
  virtual Status DeleteDir(const std::string& dirname) = 0;

  // Store the size of fname in *file_size.
  // 
  // 获取 fname 文件大小，赋值于 *file_size
  virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

  // Rename file src to target.
  // 
  // 重命名 src 文件名为 target
  virtual Status RenameFile(const std::string& src,
                            const std::string& target) = 0;

  // Lock the specified file.  Used to prevent concurrent access to
  // the same db by multiple processes.  On failure, stores NULL in
  // *lock and returns non-OK.
  //
  // On success, stores a pointer to the object that represents the
  // acquired lock in *lock and returns OK.  The caller should call
  // UnlockFile(*lock) to release the lock.  If the process exits,
  // the lock will be automatically released.
  //
  // If somebody else already holds the lock, finishes immediately
  // with a failure.  I.e., this call does not wait for existing locks
  // to go away.
  //
  // May create the named file if it does not already exist.
  // 
  // 锁定指定的文件。用于防止多个进程并发访问相同的数据库。失败时，*lock=NULL，
  // 并返回 non-OK
  // 
  // 成功时，将指向表示获取的锁的对象的指针存储在 *lock 中，并返回 OK。调用者
  // 应该调用 UnlockFile(*lock) 来释放锁。如果进程退出，锁将自动释放
  // 
  // 如果其他人已经锁定，失败立即返回。即，这个调用不会等待现有的锁消失
  // 
  // 如果指定的文件不存在，会自动创建它
  virtual Status LockFile(const std::string& fname, FileLock** lock) = 0;

  // Release the lock acquired by a previous successful call to LockFile.
  // REQUIRES: lock was returned by a successful LockFile() call
  // REQUIRES: lock has not already been unlocked.
  // 
  // 释放指定的获取的文件锁。*lock 必须是 LockFile() 调用所获得的锁，并且该锁还没
  // 有被解锁过
  virtual Status UnlockFile(FileLock* lock) = 0;

  // Arrange to run "(*function)(arg)" once in a background thread.
  //
  // "function" may run in an unspecified thread.  Multiple functions
  // added to the same Env may run concurrently in different threads.
  // I.e., the caller may not assume that background work items are
  // serialized.
  // 
  // 添加任务（ 任务子程序 "(*function)(arg)" ）到后台线程中运行一次
  // "function" 可能在未指定的线程中运行。添加到同一个 Env 的多个函数可以在
  // 不同的线程中同时运行。即，调用者可能不会认为后台工作项目是序列化的
  virtual void Schedule(
      void (*function)(void* arg),
      void* arg) = 0;

  // Start a new thread, invoking "function(arg)" within the new thread.
  // When "function(arg)" returns, the thread will be destroyed.
  // 
  // 启动一个新线程，在新线程中调用 "function(arg)"。当 "function(arg)" 返回时，
  // 线程将被销毁。"function(arg)" 也称线程为入口函数
  virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

  // *path is set to a temporary directory that can be used for testing. It may
  // or many not have just been created. The directory may or may not differ
  // between runs of the same process, but subsequent calls will return the
  // same directory.
  // 
  // *path 为获取的可用于测试的临时目录。它可能还有许多不是刚刚创建的。该目录在同一进程的
  // 运行之间可能相同也可能不同，但随后的调用将返回相同的目录
  virtual Status GetTestDirectory(std::string* path) = 0;

  // Create and return a log file for storing informational messages.
  // 
  // 创建并返回一个用于存储消息的日志文件
  virtual Status NewLogger(const std::string& fname, Logger** result) = 0;

  // Returns the number of micro-seconds since some fixed point in time. Only
  // useful for computing deltas of time.
  // 
  // 返回某个固定时间点以来的微秒数。仅用于计算时间的增量
  virtual uint64_t NowMicros() = 0;

  // Sleep/delay the thread for the prescribed number of micro-seconds.
  // 
  // Sleep/delay 线程规定的微秒数
  virtual void SleepForMicroseconds(int micros) = 0;

 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  Env(const Env&);
  void operator=(const Env&);
};

// A file abstraction for reading sequentially through a file
// 
// 顺序读取文件的抽象接口
// leveldb 中使用带缓冲区 I/O 的文件操作库
class SequentialFile {
 public:
  SequentialFile() { }
  virtual ~SequentialFile();

  // Read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  Sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // May set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // If an error was encountered, returns a non-OK status.
  //
  // REQUIRES: External synchronization
  // 
  // 从文件中读取 "n" 个字节
  // "scratch[0..n-1]" 可以通过这个函数写入。将 "*result" 设置为读取的数据（包
  // 括如果少于 "n" 字节被成功读取）。可以设置 "*result" 指向 "scratch[0..n-1]" 
  // 中的数据，所以当使用 "*result" 时，"scratch[0..n-1]" 必须是活动的（生命周期
  // 有效范围内）。如果遇到错误，则返回 non-OK
  // 
  // REQUIRES：外部同步（非线程安全）
  virtual Status Read(size_t n, Slice* result, char* scratch) = 0;

  // Skip "n" bytes from the file. This is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // If end of file is reached, skipping will stop at the end of the
  // file, and Skip will return OK.
  //
  // REQUIRES: External synchronization
  // 
  // 从文件中跳过 "n" 个字节
  // 读取相同的数据可以保证不会变慢，但可能会更快。如果文件结束，跳过将在文件结束时
  // 停止，Skip 将返回 OK
  // 
  // REQUIRES：外部同步（非线程安全）
  virtual Status Skip(uint64_t n) = 0;

 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  SequentialFile(const SequentialFile&);
  void operator=(const SequentialFile&);
};

// A file abstraction for randomly reading the contents of a file.
// 
// 随机读取文件的抽象接口
class RandomAccessFile {
 public:
  RandomAccessFile() { }
  virtual ~RandomAccessFile();

  // Read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  Sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  May set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  If an error was encountered, returns a non-OK
  // status.
  //
  // Safe for concurrent use by multiple threads.
  // 
  // 从文件 "offset" 开始读取 "n" 个字节
  // "scratch[0..n-1]" 可以通过这个函数写入。将 "*result" 设置为读取的数据（包括
  // 如果少于 "n" 字节被成功读取）。可以设置 "*result" 指向 "scratch[0..n-1]" 中
  // 的数据，所以当使用 "*result" 时，"scratch[0..n-1]" 必须是活动的（生命周期有效
  // 范围内）。如果遇到错误，则返回 non-OK
  // 
  // 多线程并发使用是安全的（线程安全，底层使用 pread 函数）
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const = 0;

 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  RandomAccessFile(const RandomAccessFile&);
  void operator=(const RandomAccessFile&);
};

// A file abstraction for sequential writing.  The implementation
// must provide buffering since callers may append small fragments
// at a time to the file.
// 
// 顺序写入文件的抽象接口。实现必须提供缓冲，因为调用者一次可以将小碎片附加
// 到文件中（leveldb 中使用带缓冲区 I/O 的文件操作库）
class WritableFile {
 public:
  WritableFile() { }
  virtual ~WritableFile();

  virtual Status Append(const Slice& data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;

 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  WritableFile(const WritableFile&);
  void operator=(const WritableFile&);
};

// An interface for writing log messages.
// 
// 用于编写日志消息的接口
class Logger {
 public:
  Logger() { }
  virtual ~Logger();

  // Write an entry to the log file with the specified format.
  // 
  // 用指定的格式写入日志文件
  virtual void Logv(const char* format, va_list ap) = 0;

 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  Logger(const Logger&);
  void operator=(const Logger&);
};


// Identifies a locked file.
// 
// 标识一个锁定的文件
class FileLock {
 public:
  FileLock() { }
  virtual ~FileLock();
 private:
  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  FileLock(const FileLock&);
  void operator=(const FileLock&);
};

// Log the specified data to *info_log if info_log is non-NULL.
// 
// 如果 info_log 是 non-NULL，则将指定的数据记录到 *info_log
// 
// __attribute__(__format__) 提示该函数使用 printf 函数调用格式
extern void Log(Logger* info_log, const char* format, ...)
#   if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__ (__printf__, 2, 3)))
#   endif
    ;

// A utility routine: write "data" to the named file.
// 
// 将 "data" 写入指定的文件(不实时刷新 I/O 缓冲区，由 OS 自身决定刷新时机)
extern Status WriteStringToFile(Env* env, const Slice& data,
                                const std::string& fname);

// A utility routine: read contents of named file into *data
// 
// 将指定文件的内容读入 *data
extern Status ReadFileToString(Env* env, const std::string& fname,
                               std::string* data);

// An implementation of Env that forwards all calls to another Env.
// May be useful to clients who wish to override just part of the
// functionality of another Env.
// 
// 将所有调用转发给另一个 Env 的实现。主要用于那些仅仅希望覆盖 Env 一部分功
// 能的客户端
class EnvWrapper : public Env {
 public:
  // Initialize an EnvWrapper that delegates all calls to *t
  // 
  // 初始化将所有调用委托给 *t 的 EnvWrapper
  explicit EnvWrapper(Env* t) : target_(t) { }
  virtual ~EnvWrapper();

  // Return the target to which this Env forwards all calls
  // 
  // 返回这个转发所有调用的 Env 对象
  Env* target() const { return target_; }

  // The following text is boilerplate that forwards all methods to target()
  // 
  // 以下内容将所有调用转发到 target() 的方法
  Status NewSequentialFile(const std::string& f, SequentialFile** r) {
    return target_->NewSequentialFile(f, r);
  }
  Status NewRandomAccessFile(const std::string& f, RandomAccessFile** r) {
    return target_->NewRandomAccessFile(f, r);
  }
  Status NewWritableFile(const std::string& f, WritableFile** r) {
    return target_->NewWritableFile(f, r);
  }
  bool FileExists(const std::string& f) { return target_->FileExists(f); }
  Status GetChildren(const std::string& dir, std::vector<std::string>* r) {
    return target_->GetChildren(dir, r);
  }
  Status DeleteFile(const std::string& f) { return target_->DeleteFile(f); }
  Status CreateDir(const std::string& d) { return target_->CreateDir(d); }
  Status DeleteDir(const std::string& d) { return target_->DeleteDir(d); }
  Status GetFileSize(const std::string& f, uint64_t* s) {
    return target_->GetFileSize(f, s);
  }
  Status RenameFile(const std::string& s, const std::string& t) {
    return target_->RenameFile(s, t);
  }
  Status LockFile(const std::string& f, FileLock** l) {
    return target_->LockFile(f, l);
  }
  Status UnlockFile(FileLock* l) { return target_->UnlockFile(l); }
  void Schedule(void (*f)(void*), void* a) {
    return target_->Schedule(f, a);
  }
  void StartThread(void (*f)(void*), void* a) {
    return target_->StartThread(f, a);
  }
  virtual Status GetTestDirectory(std::string* path) {
    return target_->GetTestDirectory(path);
  }
  virtual Status NewLogger(const std::string& fname, Logger** result) {
    return target_->NewLogger(fname, result);
  }
  uint64_t NowMicros() {
    return target_->NowMicros();
  }
  void SleepForMicroseconds(int micros) {
    target_->SleepForMicroseconds(micros);
  }
 private:
  Env* target_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_ENV_H_
