// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_DB_H_
#define STORAGE_LEVELDB_INCLUDE_DB_H_

#include <stdint.h>
#include <stdio.h>
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {

// Update Makefile if you change these
// 
// 项目版本号
static const int kMajorVersion = 1;
static const int kMinorVersion = 18;

struct Options;
struct ReadOptions;
struct WriteOptions;
class WriteBatch;

// Abstract handle to particular state of a DB.
// A Snapshot is an immutable object and can therefore be safely
// accessed from multiple threads without any external synchronization.
// 
// 抽象句柄到特定的 DB 状态
// 快照是不可变的对象，因此可以安全地从多个线程访问，而无需任何外部同步
class Snapshot {
 protected:
  virtual ~Snapshot();
};

// A range of keys
// 
// keys 范围
struct Range {
  Slice start;          // Included in the range
  Slice limit;          // Not included in the range

  Range() { }
  Range(const Slice& s, const Slice& l) : start(s), limit(l) { }
};

// A DB is a persistent ordered map from keys to values.
// A DB is safe for concurrent access from multiple threads without
// any external synchronization.
// 
// 数据库是从键到值的持久有序映射
// 一个数据库对于多线程并发访问是安全的，不需要任何外部同步
class DB {
 public:
  // Open the database with the specified "name".
  // Stores a pointer to a heap-allocated database in *dbptr and returns
  // OK on success.
  // Stores NULL in *dbptr and returns a non-OK status on error.
  // Caller should delete *dbptr when it is no longer needed.
  // 
  // 用指定的 "name" 打开数据库
  // 在 *dbptr 中存储一个指向堆分配数据库的指针，并在成功时返回 OK
  // 在 *dbptr 中存储 NULL，并在出错时返回 non-OK 状态。调用者应该在不再需要时
  // 删除 *dbptr
  static Status Open(const Options& options,
                     const std::string& name,
                     DB** dbptr);

  DB() { }
  virtual ~DB();

  // Set the database entry for "key" to "value".  Returns OK on success,
  // and a non-OK status on error.
  // Note: consider setting options.sync = true.
  // 
  // 将数据库 "key" 的条目设置为 "value"。成功时返回 OK，错误时返回 non-OK
  // 注意：考虑设置 options.sync = true
  virtual Status Put(const WriteOptions& options,
                     const Slice& key,
                     const Slice& value) = 0;

  // Remove the database entry (if any) for "key".  Returns OK on
  // success, and a non-OK status on error.  It is not an error if "key"
  // did not exist in the database.
  // Note: consider setting options.sync = true.
  // 
  // 删除数据库 "key" 的条目（如果有的话）。成功时返回 OK，错误时返回 non-OK 状态。
  // 如果数据库中不存在 "key"，则不是错误
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

  // Apply the specified updates to the database.
  // Returns OK on success, non-OK on failure.
  // Note: consider setting options.sync = true.
  // 
  // 将指定的更新应用于数据库。成功返回 OK，失败则返回 non-OK
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

  // If the database contains an entry for "key" store the
  // corresponding value in *value and return OK.
  // 
  // If there is no entry for "key" leave *value unchanged and return
  // a status for which Status::IsNotFound() returns true.
  // 
  // May return some other Status on an error.
  // 
  // 如果数据库包含 "key" 的条目，则将相应的值存储在 *value 中，然后返回 OK
  // 如果没有 "key" 的条目，保持 *value 保持不变，并返回 Status::IsNotFound()
  // 也可能会返回一些其他状态的错误
  virtual Status Get(const ReadOptions& options,
                     const Slice& key, std::string* value) = 0;

  // Return a heap-allocated iterator over the contents of the database.
  // The result of NewIterator() is initially invalid (caller must
  // call one of the Seek methods on the iterator before using it).
  //
  // Caller should delete the iterator when it is no longer needed.
  // The returned iterator should be deleted before this db is deleted.
  // 
  // 在数据库的内容上返回堆分配的迭代器。 NewIterator() 的结果最初是无效的（调用
  // 者在使用它之前必须调用迭代器上的 Seek 方法之一）
  // 
  // 调用者应该在不再需要的时候删除迭代器。在删除这个 db 之前，应该删除返回的迭代器
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;

  // Return a handle to the current DB state.  Iterators created with
  // this handle will all observe a stable snapshot of the current DB
  // state.  The caller must call ReleaseSnapshot(result) when the
  // snapshot is no longer needed.
  // 
  // 返回当前 DB 状态的句柄。使用此句柄创建的迭代器将全部观察当前 DB 状态的稳定快照。
  // 当快照不再需要时，调用者必须调用 ReleaseSnapshot(snapshot)
  virtual const Snapshot* GetSnapshot() = 0;

  // Release a previously acquired snapshot.  The caller must not
  // use "snapshot" after this call.
  // 
  // 释放之前获取的快照。此调用后，调用者不得再使用 "snapshot"
  virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

  // DB implementations can export properties about their state
  // via this method.  If "property" is a valid property understood by this
  // DB implementation, fills "*value" with its current value and returns
  // true.  Otherwise returns false.
  // 
  // 数据库实现可以通过这种方法导出关于状态的属性。如果 "property" 是这个数据库实现所
  // 理解的有效属性，则用当前值填充 "*value" 并返回 true。否则返回 false
  //
  //
  // Valid property names include:
  //
  //  "leveldb.num-files-at-level<N>" - return the number of files at level <N>,
  //     where <N> is an ASCII representation of a level number (e.g. "0").
  //  "leveldb.stats" - returns a multi-line string that describes statistics
  //     about the internal operation of the DB.
  //  "leveldb.sstables" - returns a multi-line string that describes all
  //     of the sstables that make up the db contents.
  //     
  // 有效的属性名称包括：
  // "leveldb.num-files-at-level<N>" - 返回级别为 <N> 的文件数量，其中 <N> 是级别编号
  // （例如 "0"）的 ASCII 表示
  // "leveldb.stats" - 返回一个多行字符串，用于描述有关数据库内部操作的统计信息
  // "leveldb.sstables" - 返回一个多行字符串，描述组成数据库内容的所有 sstables
  virtual bool GetProperty(const Slice& property, std::string* value) = 0;

  // For each i in [0,n-1], store in "sizes[i]", the approximate
  // file system space used by keys in "[range[i].start .. range[i].limit)".
  // 
  // 对于 [0,n-1] 中的每个i，存储在 "[range[i].start .. range[i].limit)" 中由键
  // 使用的近似文件系统空间 "sizes[i]"
  //
  // Note that the returned sizes measure file system space usage, so
  // if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size.
  // 
  // 请注意，返回的大小会衡量文件系统空间使用情况，因此如果用户数据压缩十倍，则返回的大小
  // 将为相应用户数据大小的十分之一
  //
  // The results may not include the sizes of recently written data.
  // 
  // 结果可能不包括最近写入的数据的大小
  virtual void GetApproximateSizes(const Range* range, int n,
                                   uint64_t* sizes) = 0;

  // Compact the underlying storage for the key range [*begin,*end].
  // In particular, deleted and overwritten versions are discarded,
  // and the data is rearranged to reduce the cost of operations
  // needed to access the data.  This operation should typically only
  // be invoked by users who understand the underlying implementation.
  // 
  // 压缩 [*begin,*end] 范围的 key 的底层存储。特别是，删除和覆盖版本被丢弃，
  // 数据重新排列，以降低访问数据所需的操作成本。这个操作通常只能由理解底层实现
  // 的用户调用。
  //
  // begin==NULL is treated as a key before all keys in the database.
  // end==NULL is treated as a key after all keys in the database.
  // Therefore the following call will compact the entire database:
  //    db->CompactRange(NULL, NULL);
  //    
  // begin==NULL 被视为数据库中所有之前的键。end==NULL 在数据库中的所有之后的
  // 键。因此，以下调用将压缩整个数据库：
  //    db->CompactRange(NULL, NULL);
  virtual void CompactRange(const Slice* begin, const Slice* end) = 0;

 private:
  // No copying allowed
  DB(const DB&);
  void operator=(const DB&);
};

// Destroy the contents of the specified database.
// Be very careful using this method.
// 
// 销毁指定数据库的内容
// 使用这种方法要非常小心
Status DestroyDB(const std::string& name, const Options& options);

// If a DB cannot be opened, you may attempt to call this method to
// resurrect as much of the contents of the database as possible.
// Some data may be lost, so be careful when calling this function
// on a database that contains important information.
// 
// 如果数据库无法打开，则可以尝试调用此方法以尽可能多地重新生成数据库的内容
// 有些数据可能会丢失，因此在包含重要信息的数据库上调用此函数时要小心
Status RepairDB(const std::string& dbname, const Options& options);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_DB_H_
