// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
#define STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_

#include <string>

namespace leveldb {

class Slice;

// A Comparator object provides a total order across slices that are
// used as keys in an sstable or a database.  A Comparator implementation
// must be thread-safe since leveldb may invoke its methods concurrently
// from multiple threads.
// 
// 比较器对象提供跨切片(用作 sstable 或 database 中的键)的总顺序。 比较器实现
// 必须是线程安全的，因为 leveldb 可以从多个线程同时调用它的方法
class Comparator {
 public:
  virtual ~Comparator();

  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  //   
  // 三路比较（底层使用字符串比较函数 memcmp）
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

  // The name of the comparator.  Used to check for comparator
  // mismatches (i.e., a DB created with one comparator is
  // accessed using a different comparator.
  //
  // The client of this package should switch to a new name whenever
  // the comparator implementation changes in a way that will cause
  // the relative ordering of any two keys to change.
  //
  // Names starting with "leveldb." are reserved and should not be used
  // by any clients of this package.
  // 
  // 比较器的名称。用于检查比较器不匹配（即，使用不同的比较器访问由一个比较器创建
  // 的 DB）
  // 
  // 当比较器的实现改变时，这个包的客户端应该切换到一个新的名字，这将导致任何两个键
  // 的相对顺序改变。
  // 
  // 名称以 "leveldb." 开头是保留的，不应该被这个包的任何客户使用
  virtual const char* Name() const = 0;

  // Advanced functions: these are used to reduce the space requirements
  // for internal data structures like index blocks.
  // 
  // 高级功能：这些功能用于减少索引块等内部数据结构的空间需求

  // If *start < limit, changes *start to a short string in [start,limit).
  // Simple comparator implementations may return with *start unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  // 
  // 如果 *start < limit, 则在 [start,limit) 中更改 *start 为一个短字符串
  // 简单的比较器实现可以返回 *start 不变。即，这个方法的实现不做任何事情是正确的
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const = 0;

  // Changes *key to a short string >= *key.
  // Simple comparator implementations may return with *key unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  // 
  // 将 *key 改为短字符串 >= *key
  // 简单的比较器实现可以返回 *key 不变。即，这个方法的实现不做任何事情是正确的
  virtual void FindShortSuccessor(std::string* key) const = 0;
};

// Return a builtin comparator that uses lexicographic byte-wise
// ordering.  The result remains the property of this module and
// must not be deleted.
// 
// 返回按字节顺序排列的内置比较器。结果仍然是该模块的属性，不能删除
extern const Comparator* BytewiseComparator();

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_COMPARATOR_H_
