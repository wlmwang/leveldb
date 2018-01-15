// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <deque>
#include <set>
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/posix_logger.h"

namespace leveldb {

namespace {

// 创建带有制定错误消息、错误码的对象
static Status IOError(const std::string& context, int err_number) {
  return Status::IOError(context, strerror(err_number));
}

// POSIX 平台顺序读取文件的实现
// 使用带缓冲区 I/O 的文件操作库
class PosixSequentialFile: public SequentialFile {
 private:
  // 文件名
  std::string filename_;
  // 文件句柄
  FILE* file_;

 public:
  PosixSequentialFile(const std::string& fname, FILE* f)
      : filename_(fname), file_(f) { }
  // 析构时关闭文件句柄
  virtual ~PosixSequentialFile() { fclose(file_); }

  // 从文件中读取 "n" 个字节
  // 需外部同步（非线程安全）
  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    // 二进制无锁读取 n 个字节数据（leveldb 中实际上是 fread 函数的宏）
    // 
    // @tips
    // \file <stdio.h>
    // size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
    // fread() 函数每次从 stream 中最多读取 count 个单元，每个单元大小为 size 个字节
    // 的数据。将读取的数据放到 buffer。文件流的位置指针后移 size*count 字节
    // 
    // 返回实际读取的单元个数。如果小于 count，则可能文件结束或读取出错。
    // 可以用 ferror() 检测是否读取出错，用 feof() 函数检测是否到达文件结尾。
    // 如果 size 或 count 为 0，则返回 0
    size_t r = fread_unlocked(scratch, 1, n, file_);
    // 创建基于 scratch 缓冲区的切片
    *result = Slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // We leave status as ok if we hit the end of the file
        // 
        // 如果达到文件的末尾，保持 ok 状态
      } else {
        // A partial read with an error: return a non-ok status
        // 
        // 部分读取错误：返回 non-ok
        s = IOError(filename_, errno);
      }
    }
    return s;
  }

  // 从文件中跳过 "n" 个字节
  // 需外部同步（非线程安全）
  virtual Status Skip(uint64_t n) {
    if (fseek(file_, n, SEEK_CUR)) {
      return IOError(filename_, errno);
    }
    return Status::OK();
  }
};

// pread() based random-access
// 
// POSIX 平台随机读取文件的实现。基于 pread() 的随机访问（线程安全）
class PosixRandomAccessFile: public RandomAccessFile {
 private:
  // 文件名
  std::string filename_;
  // 文件描述符
  int fd_;

 public:
  PosixRandomAccessFile(const std::string& fname, int fd)
      : filename_(fname), fd_(fd) { }
  // 析构时关闭文件句柄
  virtual ~PosixRandomAccessFile() { close(fd_); }

  // 从文件 "offset" 开始读取 "n" 个字节
  // 无需外部同步（线程安全）
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const {
    Status s;
    // 带偏移量地原子的从文件中读取数据。若读取成功则返回实际读到的字节数，若已到
    // 文件结尾则返回 0，若出错则返回 -1
    // 
    // @tips
    // \file <unistd.h>
    // ssize_t pread(int fd, void *buf, size_t count, off_t offset);
    // 1. pread 相当于先调用 lseek 接着调用 read 函数。但 pread 是原子操作，定位
    // 和读操作在一个原子操作中完成，期间不可中断
    // 2. pread 不更改当前文件的指针，也就是说不改变当前文件偏移量
    // 3. pread 中的 offset 是一个绝对量，相对于文件开始处的绝对量，与当前文件指针
    // 位置无关
    // 
    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
    // 带偏移量地原子的从文件中写入数据，若写入成功则返回写入的字节数，若出错则返回 -1
    ssize_t r = pread(fd_, scratch, n, static_cast<off_t>(offset));
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      // 
      // 发生错误，返回 non-ok
      s = IOError(filename_, errno);
    }
    return s;
  }
};

// Helper class to limit mmap file usage so that we do not end up
// running out virtual memory or running into kernel performance
// problems for very large databases.
// 
// 辅助类来限制 mmap 文件的使用，这样我们不会耗尽虚拟内存，或者运行到超大型
// 数据库的内核性能产生问题
class MmapLimiter {
 public:
  // Up to 1000 mmaps for 64-bit binaries; none for smaller pointer sizes.
  // 
  // 对于 64-bit 二进制文件，最高可达 1000 个 mmaps; 否则为 0 个
  MmapLimiter() {
    SetAllowed(sizeof(void*) >= 8 ? 1000 : 0);
  }

  // If another mmap slot is available, acquire it and return true.
  // Else return false.
  // 
  // 如果另一个 mmap 插槽可用，则获取它并返回 true。否则返回 false
  bool Acquire() {
    if (GetAllowed() <= 0) {
      return false;
    }
    MutexLock l(&mu_);
    intptr_t x = GetAllowed();
    if (x <= 0) {
      return false;
    } else {
      // 减少一个 mmap 插槽给调用者使用（可以理解 "获取锁"）
      SetAllowed(x - 1);
      return true;
    }
  }

  // Release a slot acquired by a previous call to Acquire() that returned true.
  // 
  // 释放先前调用 Acquire() 获得的插槽，返回 true（可以理解 "释放锁"）
  void Release() {
    MutexLock l(&mu_);
    SetAllowed(GetAllowed() + 1);
  }

 private:
  // mutex 锁
  port::Mutex mu_;
  // 原子类型
  port::AtomicPointer allowed_;

  // 获取文件可支持映射的数量
  intptr_t GetAllowed() const {
    return reinterpret_cast<intptr_t>(allowed_.Acquire_Load());
  }

  // REQUIRES: mu_ must be held
  // 
  // 写入文件可支持映射的数量。线程必须持有 mu_ 锁
  void SetAllowed(intptr_t v) {
    allowed_.Release_Store(reinterpret_cast<void*>(v));
  }

  // 禁止 拷贝/赋值
  MmapLimiter(const MmapLimiter&);
  void operator=(const MmapLimiter&);
};

// mmap() based random-access
// 
// POSIX 平台随机读取文件的实现。基于 mmapped 的随机访问
class PosixMmapReadableFile: public RandomAccessFile {
 private:
  // 文件名
  std::string filename_;
  // 共享区域
  void* mmapped_region_;
  size_t length_;
  // mmap 限制对象
  MmapLimiter* limiter_;

 public:
  // base[0,length-1] contains the mmapped contents of the file.
  // 
  // base[0,length-1] 包含文件的 mmapped 内容。即，创建该对象需提前映射文件
  PosixMmapReadableFile(const std::string& fname, void* base, size_t length,
                        MmapLimiter* limiter)
      : filename_(fname), mmapped_region_(base), length_(length),
        limiter_(limiter) {
  }

  // 卸载 mmapped 映射内存。释放持有的映射 "锁"
  virtual ~PosixMmapReadableFile() {
    munmap(mmapped_region_, length_);
    limiter_->Release();
  }

  // 从文件 "offset" 开始读取 "n" 个字节
  // 切片直接使用文件映射区域地址，无需 scratch 内存缓冲区
  // 
  // @TODO
  // 无需外部同步（线程安全）（根本没有写的接口，故线程安全。但可能读出的内容不是文件最新内容）
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const {
    Status s;
    if (offset + n > length_) {
      // 读取超过总文件映射大小
      *result = Slice();
      s = IOError(filename_, EINVAL);
    } else {
      // 读取并移动映射指针
      *result = Slice(reinterpret_cast<char*>(mmapped_region_) + offset, n);
    }
    return s;
  }
};

// POSIX 平台顺序写入文件的实现
// 使用带缓冲区 I/O 的文件操作库
class PosixWritableFile : public WritableFile {
 private:
  // 文件名
  std::string filename_;
  // 文件句柄
  FILE* file_;

 public:
  PosixWritableFile(const std::string& fname, FILE* f)
      : filename_(fname), file_(f) { }

  // 析构时关闭文件句柄
  ~PosixWritableFile() {
    if (file_ != NULL) {
      // Ignoring any potential errors
      // 
      // 忽略任何潜在的错误
      fclose(file_);
    }
  }

  virtual Status Append(const Slice& data) {
    // 二进制无锁写入 n 个字节数据（leveldb 中实际上是 fwrite 函数的宏）
    // 
    // @tips
    // \file <stdio.h>
    // size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
    // 函数每次向 stream 中写入 count 个单元，每个单元大小为 size 个字节数据。文件流的位置指针
    // 后移 size*count 字节。
    // 
    // 返回成功写入的单元个数。如果小于 count，则说明发生了错误，文件流错误标志位将被设置，随后可
    // 以通过 ferror() 函数判断。如果 size 或 count 的值为 0，则返回值为 0，并且文件流的位置指
    // 针保持不变
    size_t r = fwrite_unlocked(data.data(), 1, data.size(), file_);
    if (r != data.size()) {
      return IOError(filename_, errno);
    }
    return Status::OK();
  }

  // 关闭文件句柄
  virtual Status Close() {
    Status result;
    if (fclose(file_) != 0) {
      result = IOError(filename_, errno);
    }
    file_ = NULL;
    return result;
  }

  virtual Status Flush() {
    // 刷新 file_ 文件缓冲区（leveldb 中实际上是 fflush 函数的宏）
    // 
    // @tips
    // \file <stdio.h>
    // int fflush(FILE* stream);
    // 函数会强迫将缓冲区内的数据写回参数 stream 指定的文件中。如果参数 stream 为 NULL，fflush()
    // 会将所有打开的文件数据更新。fflush() 也可用于标准输入（stdin）和标准输出（stdout），用来清
    // 空标准输入输出缓冲区
    // 
    // 成功返回 0，失败返回 EOF，错误代码存于 errno 中。指定的流没有缓冲区或者只读打开时也返回 0 值
    if (fflush_unlocked(file_) != 0) {
      return IOError(filename_, errno);
    }
    return Status::OK();
  }

  // 若当前打开的文件是以 MANIFEST* 开头的文件，则将本目录的内存缓冲数据同步刷新到储存设备。确保
  // manifest 引用的新文件在文件系统中
  Status SyncDirIfManifest() {
    // 根据 filename 获取目录与文件名称
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
    Status s;
    // 文件名是否 MANIFEST* 开头
    if (basename.starts_with("MANIFEST")) {
      int fd = open(dir.c_str(), O_RDONLY);
      if (fd < 0) {
        s = IOError(dir, errno);
      } else {
        // @tips
        // \file <unistd.h>
        // int fsync(int fd);
        // 函数负责将参数 fd 所指的文件数据，由系统缓冲区写回磁盘，以确保数据同步。
        // fsync 可以保证文件的修改时间也被更新。fsync 系统调用可以使您精确的强制每次写入都被更新到磁盘中
        // 也可以使用同步（synchronous）I/O 操作打开一个文件，这将引起所有写数据都立刻被提交到磁盘中。通
        // 过在 open 中指定 O_SYNC 标志启用同步 I/O
        // 
        // 成功返回 0，失败返回 -1，错误代码存于 errno 中
        if (fsync(fd) < 0) {
          s = IOError(dir, errno);
        }
        close(fd);
      }
    }
    return s;
  }

  virtual Status Sync() {
    // Ensure new files referred to by the manifest are in the filesystem.
    // 
    // 确保 manifest 引用的新文件在文件系统中
    Status s = SyncDirIfManifest();
    if (!s.ok()) {
      return s;
    }
    if (fflush_unlocked(file_) != 0 ||
        fdatasync(fileno(file_)) != 0) {
      s = Status::IOError(filename_, strerror(errno));
    }
    return s;
  }
};

// 锁定或解锁指定文件描述符（整个文件）
static int LockOrUnlock(int fd, bool lock) {
  errno = 0;
  struct flock f;
  memset(&f, 0, sizeof(f));
  f.l_type = (lock ? F_WRLCK : F_UNLCK);
  f.l_whence = SEEK_SET;
  // 锁定/解锁整个文件
  f.l_start = 0;
  f.l_len = 0;        // Lock/unlock entire file
  return fcntl(fd, F_SETLK, &f);
}

// POSIX 平台标识一个锁定的文件结构
class PosixFileLock : public FileLock {
 public:
  // 文件描述符
  int fd_;
  // 文件名
  std::string name_;
};

// Set of locked files.  We keep a separate set instead of just
// relying on fcntrl(F_SETLK) since fcntl(F_SETLK) does not provide
// any protection against multiple uses from the same process.
// 
// 一组锁定的文件集合。我们保留一个单独的集合，而不是仅仅依赖 fcntl(F_SETLK)，
// 因为 fcntl(F_SETLK) 没有提供对来自同一进程的多次使用的保护
class PosixLockTable {
 private:
  // mutex锁
  port::Mutex mu_;
  // 锁定的文件名集合
  std::set<std::string> locked_files_;
 public:
  // 添加一个锁文件名到集合中。加入成功，返回 true。已存在，返回 false
  bool Insert(const std::string& fname) {
    MutexLock l(&mu_);
    return locked_files_.insert(fname).second;
  }
  // 从集合删除一个锁文件名
  void Remove(const std::string& fname) {
    MutexLock l(&mu_);
    locked_files_.erase(fname);
  }
};

// POSIX 平台 Env 实现
class PosixEnv : public Env {
 public:
  PosixEnv();
  // Env 对象属于 leveldb，绝不能删除
  virtual ~PosixEnv() {
    char msg[] = "Destroying Env::Default()\n";
    fwrite(msg, 1, sizeof(msg), stderr);
    abort();
  }

  // 使用指定的名称创建一个全新的顺序访问的可读文件
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) {
    // 只读方式打开带缓冲区的 I/O 文件句柄
    FILE* f = fopen(fname.c_str(), "r");
    if (f == NULL) {
      *result = NULL;
      return IOError(fname, errno);
    } else {
      // *result 中存储 PosixSequentialFile 对象指针
      *result = new PosixSequentialFile(fname, f);
      return Status::OK();
    }
  }

  // 使用指定的名称创建一个全新的随机访问的只读文件
  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) {
    *result = NULL;
    Status s;
    // 只读方式打开不带缓冲区的 I/O 文件句柄
    int fd = open(fname.c_str(), O_RDONLY);
    if (fd < 0) {
      s = IOError(fname, errno);
    } else if (mmap_limit_.Acquire()) { // 获取一个 mmap 映射（"映射锁"）
      uint64_t size;
      // 获取文件大小
      s = GetFileSize(fname, &size);
      if (s.ok()) {
        // 将整个文件映射到共享内存中
        void* base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if (base != MAP_FAILED) {
          // *result 中存储 PosixMmapReadableFile 对象指针
          *result = new PosixMmapReadableFile(fname, base, size, &mmap_limit_);
        } else {
          s = IOError(fname, errno);
        }
      }
      // 关闭文件
      close(fd);
      if (!s.ok()) {
        // 关闭失败，释放映射内存
        mmap_limit_.Release();
      }
    } else {  // 超过映射个数限制
      // *result 中存储 PosixRandomAccessFile 对象指针（直接打开文件）
      *result = new PosixRandomAccessFile(fname, fd);
    }
    return s;
  }

  // 创建一个具有指定名称的可写入的新文件的对象
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
    Status s;
    // 写入方式打开带缓冲区的 I/O 文件句柄
    FILE* f = fopen(fname.c_str(), "w");
    if (f == NULL) {
      *result = NULL;
      s = IOError(fname, errno);
    } else {
      // *result 中存储 PosixWritableFile 对象指针
      *result = new PosixWritableFile(fname, f);
    }
    return s;
  }

  // 如果文件存在，返回 true
  virtual bool FileExists(const std::string& fname) {
    return access(fname.c_str(), F_OK) == 0;
  }

  // 在 *result 中存储指定目录下子目录名称，路径名称相对 "dir"
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) {
    // 清除 result 原始内容
    result->clear();
    // 打开目录
    DIR* d = opendir(dir.c_str());
    if (d == NULL) {
      return IOError(dir, errno);
    }
    // 读取目录
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
      // 获取文件名
      result->push_back(entry->d_name);
    }
    // 关闭目录
    closedir(d);
    return Status::OK();
  }

  // 删除指定文件
  virtual Status DeleteFile(const std::string& fname) {
    Status result;
    if (unlink(fname.c_str()) != 0) {
      result = IOError(fname, errno);
    }
    return result;
  }

  // 创建指定目录
  virtual Status CreateDir(const std::string& name) {
    Status result;
    if (mkdir(name.c_str(), 0755) != 0) {
      result = IOError(name, errno);
    }
    return result;
  }

  // 删除指定目录
  virtual Status DeleteDir(const std::string& name) {
    Status result;
    if (rmdir(name.c_str()) != 0) {
      result = IOError(name, errno);
    }
    return result;
  }

  // 获取 fname 文件大小，赋值于 *file_size
  virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
    Status s;
    struct stat sbuf;
    if (stat(fname.c_str(), &sbuf) != 0) {
      *size = 0;
      s = IOError(fname, errno);
    } else {
      *size = sbuf.st_size;
    }
    return s;
  }

  // 重命名 src 文件名为 target
  virtual Status RenameFile(const std::string& src, const std::string& target) {
    Status result;
    if (rename(src.c_str(), target.c_str()) != 0) {
      result = IOError(src, errno);
    }
    return result;
  }

  // 锁定指定的文件。将指向表示获取的锁的对象的指针存储在 *lock 中
  virtual Status LockFile(const std::string& fname, FileLock** lock) {
    *lock = NULL;
    Status result;
    // 以读写方式打开文件。文件不存在，则创建
    int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
      result = IOError(fname, errno);
    } else if (!locks_.Insert(fname)) { // 加入锁定的文件集合
      // 该文件名已经加入锁定集合。即，表明该文件已被锁定
      close(fd);
      result = Status::IOError("lock " + fname, "already held by process");
    } else if (LockOrUnlock(fd, true) == -1) {  // 锁定文件操作
      // 文件锁定失败
      result = IOError("lock " + fname, errno);
      // 关闭文件，并移除锁定集合
      close(fd);
      locks_.Remove(fname);
    } else {
      // *lock 中存储 PosixFileLock 对象指针
      PosixFileLock* my_lock = new PosixFileLock;
      my_lock->fd_ = fd;
      my_lock->name_ = fname;
      *lock = my_lock;
    }
    return result;
  }

  // 释放指定的获取的文件锁
  virtual Status UnlockFile(FileLock* lock) {
    // 转换成实际类型
    PosixFileLock* my_lock = reinterpret_cast<PosixFileLock*>(lock);
    Status result;
    if (LockOrUnlock(my_lock->fd_, false) == -1) {  // 释放文件操作
      result = IOError("unlock", errno);
    }
    // 移除锁定集合，并关闭文件
    locks_.Remove(my_lock->name_);
    close(my_lock->fd_);
    delete my_lock;
    return result;
  }

  // 添加任务（ 任务子程序 "(*function)(arg)" ）到后台线程中运行一次
  virtual void Schedule(void (*function)(void*), void* arg);

  // 启动一个新线程，在新线程中调用 "function(arg)"
  // "function(arg)" 也称为线程入口函数
  virtual void StartThread(void (*function)(void* arg), void* arg);

  // *result 为获取的可用于测试的临时目录
  virtual Status GetTestDirectory(std::string* result) {
    // 获取临时测试目录 TEST_TMPDIR 的环境变量
    const char* env = getenv("TEST_TMPDIR");
    if (env && env[0] != '\0') {
      *result = env;
    } else {
      // 未设置临时测试目录的环境变量，统一放在 /tmp/leveldbtest-euid 中
      char buf[100];
      snprintf(buf, sizeof(buf), "/tmp/leveldbtest-%d", int(geteuid()));
      *result = buf;
    }
    // Directory may already exist
    // 
    // 创建可能已经存在的目录
    CreateDir(*result);
    return Status::OK();
  }

  // 获取线程 tid
  static uint64_t gettid() {
    pthread_t tid = pthread_self();
    uint64_t thread_id = 0;
    // @TODO
    // 要如此赋值么？
    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
    return thread_id;
  }

  // 创建并返回一个用于存储消息的日志文件
  virtual Status NewLogger(const std::string& fname, Logger** result) {
    // 写入方式打开带缓冲区的 I/O 文件句柄
    FILE* f = fopen(fname.c_str(), "w");
    if (f == NULL) {
      *result = NULL;
      return IOError(fname, errno);
    } else {
      // *result 中存储 PosixLogger 对象指针
      *result = new PosixLogger(f, &PosixEnv::gettid);
      return Status::OK();
    }
  }

  // 返回某个固定时间点以来的微秒数。仅用于计算时间的增量
  virtual uint64_t NowMicros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
  }

  // Sleep/delay 线程规定的微秒数
  virtual void SleepForMicroseconds(int micros) {
    usleep(micros);
  }

 private:
  // pthread 线程函数调用统一校验函数。凡是返回非 0 都将 abort() 程序
  void PthreadCall(const char* label, int result) {
    if (result != 0) {
      fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
      abort();
    }
  }

  // BGThread() is the body of the background thread
  // 
  // BGThread() 是后台线程的主体，本质上是线程入口函数的代理函数
  // BGThreadWrapper() 为线程入口函数。由 pthread_create 直接调用
  void BGThread();
  static void* BGThreadWrapper(void* arg) {
    reinterpret_cast<PosixEnv*>(arg)->BGThread();
    return NULL;
  }

  // 后台任务线程控制成员
  pthread_mutex_t mu_;  // 后台线程是否启动、任务队列互斥锁
  pthread_cond_t bgsignal_; // 后台线程是否可以添加任务的条件变量
  pthread_t bgthread_;  // 后台线程 tid
  bool started_bgthread_; // 后台线程是否已启动

  // Entry per Schedule() call
  // 
  // queue_ 为 每个 Schedule() 调用的实体队列
  struct BGItem { void* arg; void (*function)(void*); };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;

  // 文件锁定集合
  PosixLockTable locks_;
  // 随机读取文件 mmap 个数限制
  MmapLimiter mmap_limit_;
};

PosixEnv::PosixEnv() : started_bgthread_(false) {
  // 初始化锁、条件变量
  PthreadCall("mutex_init", pthread_mutex_init(&mu_, NULL));
  PthreadCall("cvar_init", pthread_cond_init(&bgsignal_, NULL));
}

// 添加任务（ 任务子程序 "(*function)(arg)" ）到后台线程中运行一次
void PosixEnv::Schedule(void (*function)(void*), void* arg) {
  // 先上锁
  PthreadCall("lock", pthread_mutex_lock(&mu_));

  // Start background thread if necessary
  // 
  // 如有必要，先启动后台线程。并调用入口函数 PosixEnv::BGThreadWrapper
  if (!started_bgthread_) {
    started_bgthread_ = true;
    PthreadCall(
        "create thread",
        pthread_create(&bgthread_, NULL,  &PosixEnv::BGThreadWrapper, this));
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  // 
  // 如果队列当前为空，后台线程可能正在等待
  // 1. 后台线程可能是第一次创建，需等待创建完成
  // 2. 后台线程本身也只能一个一个的任务添加
  if (queue_.empty()) {
    PthreadCall("signal", pthread_cond_signal(&bgsignal_));
  }

  // Add to priority queue
  // 
  // 添加到任务队列末尾
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  // 解锁
  PthreadCall("unlock", pthread_mutex_unlock(&mu_));
}

// BGThread() 是后台线程的主体，本质上是线程入口函数的代理函数
void PosixEnv::BGThread() {
  while (true) {
    // Wait until there is an item that is ready to run
    // 
    // 等待直到有一个任务准备好运行
    PthreadCall("lock", pthread_mutex_lock(&mu_));
    while (queue_.empty()) {
      // 任务为空，通知 Schedule() 可以添加新任务
      PthreadCall("wait", pthread_cond_wait(&bgsignal_, &mu_));
    }

    // 从队列头部取出一个任务，并删除
    void (*function)(void*) = queue_.front().function;  // 获取函数指针
    void* arg = queue_.front().arg;
    queue_.pop_front();

    // 解锁
    PthreadCall("unlock", pthread_mutex_unlock(&mu_));
    // 运行该任务函数
    (*function)(arg);
  }
}

// 启动一个新线程结构体
namespace {
struct StartThreadState {
  void (*user_function)(void*); // 用户函数（线程入口函数）
  void* arg;  // 用户函数参数
};
}
static void* StartThreadWrapper(void* arg) {
  StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
  // 运行用户函数
  state->user_function(state->arg);
  // 清除申请的内存
  delete state;
  return NULL;
}

// 启动一个新线程，在新线程中调用 "function(arg)"
void PosixEnv::StartThread(void (*function)(void* arg), void* arg) {
  pthread_t t;
  // 创建一个启动线程结构体
  StartThreadState* state = new StartThreadState;
  state->user_function = function;
  state->arg = arg;
  // 创建线程，运行线程入口函数 StartThreadWrapper
  PthreadCall("start thread",
              pthread_create(&t, NULL,  &StartThreadWrapper, state));
}

}  // namespace

// @tips
// \file <pthread.h>
// int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))；
// 函数使用初值为 PTHREAD_ONCE_INIT 的 once_control 变量，保证 init_routine() 函数在本进程执行序列
// 中仅执行一次
// Linux 中使用互斥锁和条件变量保证由 pthread_once() 指定的函数执行且仅执行一次，而 once_control 表示
// 是否执行过
// 在 Linux 中，"一次性函数" 的执行状态有三种：NEVER(0)/IN_PROGRESS(1)/DONE(2)，如果 once_control
// 初值设为 1，则由于所有 pthread_once() 都必须等待其中一个激发 "已执行一次" 信号，即所有 pthread_once()
// 都会陷入永久的等待中；如果设为 2，则表示该函数已执行过一次，从而所有 pthread_once() 都会立即返回 0
static pthread_once_t once = PTHREAD_ONCE_INIT;
static Env* default_env;
static void InitDefaultEnv() { default_env = new PosixEnv; }

// 返回适合当前操作系统的默认环境。 Default() 的结果属于 leveldb 进程的，绝不能删除
Env* Env::Default() {
  pthread_once(&once, InitDefaultEnv);
  return default_env;
}

}  // namespace leveldb
