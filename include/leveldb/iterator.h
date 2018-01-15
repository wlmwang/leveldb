// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An iterator yields a sequence of key/value pairs from a source.
// The following class defines the interface.  Multiple implementations
// are provided by this library.  In particular, iterators are provided
// to access the contents of a Table or a DB.
// 
// 迭代器从一个源生成一系列的键/值对，iterator 定义了接口。这个库提供了多个实现。特别
// 提供迭代器来访问 table 或 DB 的内容。
//
// Multiple threads can invoke const methods on an Iterator without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Iterator must use
// external synchronization.
// 
// 多个线程可以在 Iterator 上调用 const 方法而无需外部同步，但是如果任何线程可能调用
// non-const 方法，则访问相同 Iterator 的所有线程都必须使用外部同步

#ifndef STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
#define STORAGE_LEVELDB_INCLUDE_ITERATOR_H_

#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class Iterator {
 public:
  Iterator();
  virtual ~Iterator();

  // An iterator is either positioned at a key/value pair, or
  // not valid.  This method returns true iff the iterator is valid.
  // 
  // 迭代器要么定位在键/值对上，要么是无效的。如果迭代器有效，则此方法返回 true
  virtual bool Valid() const = 0;

  // Position at the first key in the source.  The iterator is Valid()
  // after this call iff the source is not empty.
  // 
  // 定位到 key 源开始位置。如果源不是空的，则在此调用之后迭代器为 Valid()
  virtual void SeekToFirst() = 0;

  // Position at the last key in the source.  The iterator is
  // Valid() after this call iff the source is not empty.
  // 
  // 定位到 key 源结尾位置。如果源不是空的，则在此调用之后迭代器为 Valid()
  virtual void SeekToLast() = 0;

  // Position at the first key in the source that at or past target
  // The iterator is Valid() after this call iff the source contains
  // an entry that comes at or past target.
  // 
  // 定位到源或目标源中的第一个键，迭代器在此调用之后是 Valid() ，如果源包含进入
  // 或超出目标的条目
  virtual void Seek(const Slice& target) = 0;

  // Moves to the next entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the last entry in the source.
  // REQUIRES: Valid()
  // 
  // 移到源中的下一个条目。在这个调用之后，如果迭代器没有被定位在源的最后一个入口，则 
  // Valid() 为 true
  virtual void Next() = 0;

  // Moves to the previous entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the first entry in source.
  // REQUIRES: Valid()
  // 
  // 移到源中的上一个条目。在这个调用之后，如果迭代器没有被定位在源的最后一个入口，则 
  // Valid() 为 true
  virtual void Prev() = 0;

  // Return the key for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  // 
  // 返回当前条目的 key。返回分片的底层存储，只有在迭代器的下一次修改之前才有效
  virtual Slice key() const = 0;

  // Return the value for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  // 
  // 返回当前条目的 key。返回分片的底层存储，只有在迭代器的下一次修改之前才有效
  virtual Slice value() const = 0;

  // If an error has occurred, return it.  Else return an ok status.
  // 
  // 如果迭代发生错误，请将其退回。否则返回一个 ok 状态
  virtual Status status() const = 0;

  // Clients are allowed to register function/arg1/arg2 triples that
  // will be invoked when this iterator is destroyed.
  // 
  // 客户端被允许注册 function/arg1/arg2 三元组，当这个迭代器被销毁时将被调用
  //
  // Note that unlike all of the preceding methods, this method is
  // not abstract and therefore clients should not override it.
  // 
  // 注册清理函数。此方法不是抽象的，因此客户端不应该重写它
  typedef void (*CleanupFunction)(void* arg1, void* arg2);
  void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

 private:
  struct Cleanup {
    CleanupFunction function;
    void* arg1;
    void* arg2;
    Cleanup* next;
  };
  // 清理函数链表。析构时依次调用 function(arg1,arg2), 并 delete 每个链表节点
  Cleanup cleanup_;

  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  Iterator(const Iterator&);
  void operator=(const Iterator&);
};

// Return an empty iterator (yields nothing).
// 
// 返回一个空的迭代器（不产生任何东西）
extern Iterator* NewEmptyIterator();

// Return an empty iterator with the specified status.
// 
// 返回一个空指定状态的迭代器
extern Iterator* NewErrorIterator(const Status& status);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
