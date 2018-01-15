// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A database can be configured with a custom FilterPolicy object.
// This object is responsible for creating a small filter from a set
// of keys.  These filters are stored in leveldb and are consulted
// automatically by leveldb to decide whether or not to read some
// information from disk. In many cases, a filter can cut down the
// number of disk seeks form a handful to a single disk seek per
// DB::Get() call.
//
// Most people will want to use the builtin bloom filter support (see
// NewBloomFilterPolicy() below).
// 
// 数据库可以使用自定义的 FilterPolicy 对象进行配置。该对象负责从一组键中创建一个小型
// 过滤器。这些过滤器存储在 leveldb 中，由 leveldb 自动查询以决定是否从磁盘读取一些信
// 息。在许多情况下，过滤器可以减少磁盘寻道的数量，从而使得每个 DB::Get() 调用都能够从
// 单个磁盘寻道
// 
// 大多数人会想要使用内置布隆过滤器支持（参见 NewBloomFilterPolicy()）
// 
// 总体来说，过滤器与关系型数据库中的索引概念有点类似用途

#ifndef STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
#define STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_

#include <string>

namespace leveldb {

class Slice;

class FilterPolicy {
 public:
  virtual ~FilterPolicy();

  // Return the name of this policy.  Note that if the filter encoding
  // changes in an incompatible way, the name returned by this method
  // must be changed.  Otherwise, old incompatible filters may be
  // passed to methods of this type.
  // 
  // 返回此过滤器规则的名称。请注意，如果过滤器编码以不兼容的方式更改，则必须更改
  // 此方法返回的名称。否则，旧的不兼容的过滤器可能被传递给这种类型的方法
  virtual const char* Name() const = 0;

  // keys[0,n-1] contains a list of keys (potentially with duplicates)
  // that are ordered according to the user supplied comparator.
  // Append a filter that summarizes keys[0,n-1] to *dst.
  //
  // Warning: do not change the initial contents of *dst.  Instead,
  // append the newly constructed filter to *dst.
  // 
  // keys[0,n-1] 包含按照用户提供的比较器排序的键列表（可能具有重复）
  // 追加过滤器规则，用于摘要 keys[0,n-1] 到 *dst
  // 
  // 警告：请勿更改 *dst 的初始内容。相反，将新构建的过滤器追加到 *dst
  virtual void CreateFilter(const Slice* keys, int n, std::string* dst)
      const = 0;

  // "filter" contains the data appended by a preceding call to
  // CreateFilter() on this class.  This method must return true if
  // the key was in the list of keys passed to CreateFilter().
  // This method may return true or false if the key was not on the
  // list, but it should aim to return false with a high probability.
  // 
  // "filter" 包含由此类上的 CreateFilter() 的前面调用附加的数据。如果键位于
  // 传递给 CreateFilter() 的键列表中，则此方法必须返回 true。如果 key 不在
  // 列表中，则此方法可能会返回 true 或 false，但应该以高概率返回 false
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

// Return a new filter policy that uses a bloom filter with approximately
// the specified number of bits per key.  A good value for bits_per_key
// is 10, which yields a filter with ~ 1% false positive rate.
//
// Callers must delete the result after any database that is using the
// result has been closed.
//
// Note: if you are using a custom comparator that ignores some parts
// of the keys being compared, you must not use NewBloomFilterPolicy()
// and must provide your own FilterPolicy that also ignores the
// corresponding parts of the keys.  For example, if the comparator
// ignores trailing spaces, it would be incorrect to use a
// FilterPolicy (like NewBloomFilterPolicy) that does not ignore
// trailing spaces in keys.
// 
// 返回一个新的过滤器策略，该过滤器策略使用布隆过滤器，每个 key 具有大约指定的
// 位数。当 bits_per_key 的取值为 10 时，产生了一个具有 ~1％ 误报率
// 
// 在任何使用 result 的数据库关闭后，调用者必须删除 result
// 
// 注意：如果你使用的自定义比较器要忽略比较键的某些部分，则不得使用 NewBloomFilterPolicy()，
// 并且必须提供您自己的 FilterPolicy ，同时也会忽略键的相应部分。
// 例如，如果比较器忽略尾随空格，则使用 FilterPolicy（如 NewBloomFilterPolicy ）将不会忽
// 略键中的尾随空格
extern const FilterPolicy* NewBloomFilterPolicy(int bits_per_key);

}

#endif  // STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
