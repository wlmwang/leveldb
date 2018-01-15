// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Endian-neutral encoding:
// * Fixed-length numbers are encoded with least-significant byte first
// * In addition we support variable length "varint" encoding
// * Strings are encoded prefixed by their length in varint format
// 
// 带字节序的整型编/解码：
// 固定长度的数字先用最不重要的字节编码
// 另外我们支持可变长度 "varint" 编码
// 字符串以 varint 格式的长度前缀编码

#ifndef STORAGE_LEVELDB_UTIL_CODING_H_
#define STORAGE_LEVELDB_UTIL_CODING_H_

#include <stdint.h>
#include <string.h>
#include <string>
#include "leveldb/slice.h"
#include "port/port.h"

namespace leveldb {

// Standard Put... routines append to a string
// 
// 整型值固定/变长编码。附加一个整型到字符串 dst 中
extern void PutFixed32(std::string* dst, uint32_t value);
extern void PutFixed64(std::string* dst, uint64_t value);
extern void PutVarint32(std::string* dst, uint32_t value);
extern void PutVarint64(std::string* dst, uint64_t value);
// 将字符串切片 value 的长度，按变长整型编码后，附加到字符串 dst 中
// | varint(slice len) | slice |
extern void PutLengthPrefixedSlice(std::string* dst, const Slice& value);

// Standard Get... routines parse a value from the beginning of a Slice
// and advance the slice past the parsed value.
// 
// 变长编码整型值解码。从一个 Slice 的开始处解析一个整型，并将该切片移动/删除已解析
// 的整型长度字符。解析后的值写入 value 中
extern bool GetVarint32(Slice* input, uint32_t* value);
extern bool GetVarint64(Slice* input, uint64_t* value);
// 解析带 "变长长度" 头部的字符串切片 input（编码查看 PutLengthPrefixedSlice），存
// 至 result 结果（已删除长度字段）
// | varint(slice len) | slice |
extern bool GetLengthPrefixedSlice(Slice* input, Slice* result);

// Pointer-based variants of GetVarint...  These either store a value
// in *v and return a pointer just past the parsed value, or return
// NULL on error.  These routines only look at bytes in the range
// [p..limit-1]
// 
// 变长编码整型值解码。将变量或者将值存储在 *v 中，并返回刚刚经过解析值的指针，或者
// 在错误时返回 NULL。这些函数只查看 [p..limit-1] 范围中的字节
extern const char* GetVarint32Ptr(const char* p,const char* limit, uint32_t* v);
extern const char* GetVarint64Ptr(const char* p,const char* limit, uint64_t* v);

// Returns the length of the varint32 or varint64 encoding of "v"
// 
// 返回 "v" 的 varint32 或 varint64 编码的长度
extern int VarintLength(uint64_t v);

// Lower-level versions of Put... that write directly into a character buffer
// REQUIRES: dst has enough space for the value being written
// 
// 整型值固定长度编码。Put... 的底层直接写入字符缓冲区的实现
extern void EncodeFixed32(char* dst, uint32_t value);
extern void EncodeFixed64(char* dst, uint64_t value);

// Lower-level versions of Put... that write directly into a character buffer
// and return a pointer just past the last byte written.
// REQUIRES: dst has enough space for the value being written
// 
// Put... 的底层直接写入字符缓冲区，并返回刚刚写入 dst 的最后一个字节的指针
// REQUIRES: dst 有足够的空间来写入价值
extern char* EncodeVarint32(char* dst, uint32_t value);
extern char* EncodeVarint64(char* dst, uint64_t value);

// Lower-level versions of Get... that read directly from a character buffer
// without any bounds checking.
// 
// 固定长度整型值解码底层实现。直接从字符缓冲区中读取，没有任何边界检查

inline uint32_t DecodeFixed32(const char* ptr) {
  if (port::kLittleEndian) {
    // Load the raw bytes
    // 
    // 加载原始字节。小端序，即整型标准编码
    uint32_t result;
    // gcc 优化这个到一个普通的加载
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
  }
}

inline uint64_t DecodeFixed64(const char* ptr) {
  if (port::kLittleEndian) {
    // Load the raw bytes
    // 
    // 加载原始字节。小端序，即整型标准编码
    uint64_t result;
    // gcc 优化这个到一个普通的加载
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    uint64_t lo = DecodeFixed32(ptr);
    uint64_t hi = DecodeFixed32(ptr + 4);
    return (hi << 32) | lo;
  }
}

// Internal routine for use by fallback path of GetVarint32Ptr
// 
// 由 GetVarint32Ptr 的 fallback 使用的内部函数
// 
// 32-bits 变长编码整型值解码。返回跳过已解析字符串指针
extern const char* GetVarint32PtrFallback(const char* p,
                                          const char* limit,
                                          uint32_t* value);
// 32-bits 变长编码整型值解码。返回跳过已解析字符串指针
inline const char* GetVarint32Ptr(const char* p,
                                  const char* limit,
                                  uint32_t* value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      // 低于 7 位。解析后 p 跳过该 1 个字节长度
      *value = result;
      return p + 1;
    }
  }
  return GetVarint32PtrFallback(p, limit, value);
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_CODING_H_
