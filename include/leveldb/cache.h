// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A Cache is an interface that maps keys to values.  It has internal
// synchronization and may be safely accessed concurrently from
// multiple threads.  It may automatically evict entries to make room
// for new entries.  Values have a specified charge against the cache
// capacity.  For example, a cache where the values are variable
// length strings, may use the length of the string as the charge for
// the string.
// 
// @TODO
// Cache 是将键映射到值的接口。它具有内部同步，可以从多个线程同时安全地访问。它可能会自动
// 驱逐条目，为新条目腾出空间。值对缓存容量有指定的费用。例如，值为可变长度字符串的缓存可以
// 使用字符串的长度作为字符串的费用
//
// A builtin cache implementation with a least-recently-used eviction
// policy is provided.  Clients may use their own implementations if
// they want something more sophisticated (like scan-resistance, a
// custom eviction policy, variable cache sizing, etc.)
// 
// leveldb 提供了具有最近最少使用(LRU) 的逐出策略的内置高速缓存实现。如果想要更复杂的特
// 性（如扫描电阻，自定义逐出策略，变量缓存大小等），客户可以使用自己的实现

#ifndef STORAGE_LEVELDB_INCLUDE_CACHE_H_
#define STORAGE_LEVELDB_INCLUDE_CACHE_H_

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

class Cache;

// Create a new cache with a fixed size capacity.  This implementation
// of Cache uses a least-recently-used eviction policy.
// 
// 创建一个具有固定大小容量的新缓存。这个实现使用了最近最少使用的驱逐策略
extern Cache* NewLRUCache(size_t capacity);

// Cache 是将键映射到值的接口
class Cache {
 public:
  Cache() { }

  // Destroys all existing entries by calling the "deleter"
  // function that was passed to the constructor.
  // 
  // 通过调用传递给构造函数的 "deleter" 函数销毁所有现有的条目
  virtual ~Cache();

  // Opaque handle to an entry stored in the cache.
  // 
  // 存储在缓存中的条目的不透明结构
  struct Handle { };

  // Insert a mapping from key->value into the cache and assign it
  // the specified charge against the total cache capacity.
  //
  // Returns a handle that corresponds to the mapping.  The caller
  // must call this->Release(handle) when the returned mapping is no
  // longer needed.
  //
  // When the inserted entry is no longer needed, the key and
  // value will be passed to "deleter".
  // 
  // 添加一个 key->value 映射到缓存中，并为缓存总容量分配指定的费用
  // 返回对应于映射的句柄。当返回的映射不再需要时，调用者必须调用 this->Release(handle)
  // 当不再需要插入的条目时，键和值将被传递给 "deleter"
  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) = 0;

  // If the cache has no mapping for "key", returns NULL.
  //
  // Else return a handle that corresponds to the mapping.  The caller
  // must call this->Release(handle) when the returned mapping is no
  // longer needed.
  // 
  // 如果缓存没有 "key" 映射，则返回 NULL。否则返回一个对应于映射的句柄。当返回的
  // 映射不再需要时，调用者必须调用 this->Release(handle)
  virtual Handle* Lookup(const Slice& key) = 0;

  // Release a mapping returned by a previous Lookup().
  // REQUIRES: handle must not have been released yet.
  // REQUIRES: handle must have been returned by a method on *this.
  // 
  // 释放由 Lookup() 返回的映射
  // REQUIRES: 句柄还未被释放过
  // REQUIRES: 句柄必须已经被 *this 上的方法返回
  virtual void Release(Handle* handle) = 0;

  // Return the value encapsulated in a handle returned by a
  // successful Lookup().
  // REQUIRES: handle must not have been released yet.
  // REQUIRES: handle must have been returned by a method on *this.
  // 
  // 返回由成功的 Lookup() 返回的句柄中封装的值
  // REQUIRES: 句柄还未被释放过
  // REQUIRES: 句柄必须已经被 *this 上的方法返回
  virtual void* Value(Handle* handle) = 0;

  // If the cache contains entry for key, erase it.  Note that the
  // underlying entry will be kept around until all existing handles
  // to it have been released.
  // 
  // 如果缓存中包含 key 的条目，请将其删除。请注意，底层条目将被保留，直到所有现有的
  // 句柄被释放
  virtual void Erase(const Slice& key) = 0;

  // Return a new numeric id.  May be used by multiple clients who are
  // sharing the same cache to partition the key space.  Typically the
  // client will allocate a new id at startup and prepend the id to
  // its cache keys.
  // 
  // 返回一个新的数字 ID。可以由共享相同缓存的多个客户端使用来对 key 空间进行分区。通
  // 常情况下，客户端将在启动时分配一个新的 ID，并将 ID 前置到其缓存键
  virtual uint64_t NewId() = 0;

 private:
  // LRU 控制实现函数
  void LRU_Remove(Handle* e);
  void LRU_Append(Handle* e);
  void Unref(Handle* e);

  struct Rep;
  Rep* rep_;

  // No copying allowed
  // 
  // 禁止 拷贝/赋值
  Cache(const Cache&);
  void operator=(const Cache&);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_CACHE_H_
