// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_CRC32C_H_
#define STORAGE_LEVELDB_UTIL_CRC32C_H_

#include <stddef.h>
#include <stdint.h>

namespace leveldb {
namespace crc32c {

// Return the crc32c of concat(A, data[0,n-1]) where init_crc is the
// crc32c of some string A.  Extend() is often used to maintain the
// crc32c of a stream of data.
// 
// 返回 concat(A, data[0,n-1]) 的 crc32c，其中 init_crc 是某个字符串 A 的 crc32c
// Extend() 通常用于维护一个数据流的 crc32c
extern uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

// Return the crc32c of data[0,n-1]
// 
// 返回 data[0,n-1] 的 crc32c
inline uint32_t Value(const char* data, size_t n) {
  return Extend(0, data, n);
}

// 掩码魔数
static const uint32_t kMaskDelta = 0xa282ead8ul;

// Return a masked representation of crc.
//
// Motivation: it is problematic to compute the CRC of a string that
// contains embedded CRCs.  Therefore we recommend that CRCs stored
// somewhere (e.g., in files) should be masked before being stored.
// 
// 返回一个被屏蔽的 crc 表示
// 
// 动机：计算包含嵌入式 CRC 的字符串的 CRC 是有问题的。因此，我们建议存储在某处（例如，
// 在文件中）的 CRC 在存储之前应该被屏蔽
inline uint32_t Mask(uint32_t crc) {
  // Rotate right by 15 bits and add a constant.
  // 
  // 向右旋转 15 位并添加一个常量
  return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

// Return the crc whose masked representation is masked_crc.
// 
// 返回其掩码表示为 masked_crc 的 crc
inline uint32_t Unmask(uint32_t masked_crc) {
  uint32_t rot = masked_crc - kMaskDelta;
  return ((rot >> 17) | (rot << 15));
}

}  // namespace crc32c
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_CRC32C_H_
