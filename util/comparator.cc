// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <algorithm>
#include <stdint.h>
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"

namespace leveldb {

Comparator::~Comparator() { }

namespace {
class BytewiseComparatorImpl : public Comparator {
 public:
  BytewiseComparatorImpl() { }

  // 比较器的名称
  virtual const char* Name() const {
    return "leveldb.BytewiseComparator";
  }

  // 三路比较（底层使用字符串比较函数 memcmp）
  virtual int Compare(const Slice& a, const Slice& b) const {
    return a.compare(b);
  }

  // 如果 *start < limit, 则在 [start,limit) 中更改 *start 为一个短字符串
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const {
    // Find length of common prefix
    // 
    // 查找 *start 与 limit 公共前缀长度
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;
    }

    if (diff_index >= min_length) {
      // Do not shorten if one string is a prefix of the other
      // 
      // 如果一个字符串是另一个字符串的前缀，则不要缩短
    } else {
      // 尝试执行 (*start)[diff_index]++, 设置 start 长度为 diff_index+1
      // 条件：第一个不同字符 < 0xff && 该字符+1 < limit[diff_index]
      uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
      if (diff_byte < static_cast<uint8_t>(0xff) &&
          diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
        (*start)[diff_index]++;
        start->resize(diff_index + 1);
        assert(Compare(*start, limit) < 0);
      }
    }
  }

  // 将 *key 改为短字符串 >= *key
  virtual void FindShortSuccessor(std::string* key) const {
    // Find first character that can be incremented
    // 
    // 找到可以递增的第一个字符
    size_t n = key->size();
    for (size_t i = 0; i < n; i++) {
      const uint8_t byte = (*key)[i];
      if (byte != static_cast<uint8_t>(0xff)) {
        (*key)[i] = byte + 1;
        key->resize(i+1);
        return;
      }
    }
    // *key is a run of 0xffs.  Leave it alone.
  }
};
}  // namespace

// 返回按字节顺序排列的内置比较器。结果仍然是该模块的属性，不能删除
static port::OnceType once = LEVELDB_ONCE_INIT;
static const Comparator* bytewise;

static void InitModule() {
  bytewise = new BytewiseComparatorImpl;
}

const Comparator* BytewiseComparator() {
  port::InitOnce(&once, InitModule);
  return bytewise;
}

}  // namespace leveldb
