// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/coding.h"

namespace leveldb {

// 32-bits 整型值固定长度编码（系统字节序编码）
void EncodeFixed32(char* buf, uint32_t value) {
  if (port::kLittleEndian) {
    // 加载原始字节。小端序，即整型标准编码
    // gcc 优化这个到一个普通的加载
    memcpy(buf, &value, sizeof(value));
  } else {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
  }
}

// 64-bits 整型值固定长度编码（系统字节序编码）
void EncodeFixed64(char* buf, uint64_t value) {
  if (port::kLittleEndian) {
    // 加载原始字节。小端序，即整型标准编码
    // gcc 优化这个到一个普通的加载
    memcpy(buf, &value, sizeof(value));
  } else {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    buf[4] = (value >> 32) & 0xff;
    buf[5] = (value >> 40) & 0xff;
    buf[6] = (value >> 48) & 0xff;
    buf[7] = (value >> 56) & 0xff;
  }
}

// 32-bits 整型值固定长度编码（系统字节序编码）
void PutFixed32(std::string* dst, uint32_t value) {
  char buf[sizeof(value)];
  EncodeFixed32(buf, value);
  // 附加编码值到字符串结尾
  dst->append(buf, sizeof(buf));
}

// 64-bits 整型值固定长度编码（系统字节序编码）
void PutFixed64(std::string* dst, uint64_t value) {
  char buf[sizeof(value)];
  EncodeFixed64(buf, value);
  // 附加编码值到字符串结尾
  dst->append(buf, sizeof(buf));
}

// 32-bits 整型值变长编码（整型标准序编码）
char* EncodeVarint32(char* dst, uint32_t v) {
  // Operate on characters as unsigneds
  // 
  // 将字符作为无符号进行操作
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  // 2^7 = 128
  static const int B = 128;
  // 用 7 的倍数次幂作为阈值，2^8 = 256 已经多余 1 字节
  if (v < (1<<7)) {
    *(ptr++) = v;
  } else if (v < (1<<14)) {
    *(ptr++) = v | B;
    *(ptr++) = v>>7;
  } else if (v < (1<<21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = v>>14;
  } else if (v < (1<<28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = (v>>14) | B;
    *(ptr++) = v>>21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = (v>>14) | B;
    *(ptr++) = (v>>21) | B;
    *(ptr++) = v>>28;
  }
  return reinterpret_cast<char*>(ptr);
}

// 32-bits 整型值变长编码
void PutVarint32(std::string* dst, uint32_t v) {
  char buf[5];
  char* ptr = EncodeVarint32(buf, v);
  // 附加编码值到字符串结尾
  dst->append(buf, ptr - buf);
}

// 64-bits 整型值变长编码（整型标准序编码）
char* EncodeVarint64(char* dst, uint64_t v) {
  // 2^7 = 128
  static const int B = 128;
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  // 用 7 的倍数次幂作为阈值，2^8 = 256 已经多余 1 字节
  while (v >= B) {
    *(ptr++) = (v & (B-1)) | B; // 先高位清零，在保留低 7 位
    v >>= 7;
  }
  *(ptr++) = static_cast<unsigned char>(v);
  return reinterpret_cast<char*>(ptr);
}

// 64-bits 整型值变长编码
void PutVarint64(std::string* dst, uint64_t v) {
  char buf[10];
  char* ptr = EncodeVarint64(buf, v);
  // 附加编码值到字符串结尾
  dst->append(buf, ptr - buf);
}

// 将字符串切片 value 的长度，按变长整型编码后，附加到字符串 dst 中
// | varint(slice len) | slice |
void PutLengthPrefixedSlice(std::string* dst, const Slice& value) {
  // 拷贝 "切片长度 + 切片字符串" 到 dst 中
  PutVarint32(dst, value.size());
  dst->append(value.data(), value.size());
}

// 返回 "v" 的 varint32 或 varint64 编码的长度
int VarintLength(uint64_t v) {
  int len = 1;
  while (v >= 128) {
    v >>= 7;
    len++;
  }
  return len;
}

// 32-bits 变长编码整型值解码
const char* GetVarint32PtrFallback(const char* p,
                                   const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      // 
      // 有更多的字节存在（即，> 128）
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return NULL;
}

// 32-bits 变长编码整型值解码。并将该切片移动/删除已解析的整型长度字符。解析后的值
// 写入 value 中
bool GetVarint32(Slice* input, uint32_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = GetVarint32Ptr(p, limit, value);
  if (q == NULL) {
    return false;
  } else {
    // 移动/删除已解析的整型长度字符
    *input = Slice(q, limit - q);
    return true;
  }
}

// 64-bits 变长编码整型值解码。返回跳过已解析字符串指针
const char* GetVarint64Ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      // 
      // 有更多的字节存在（即，> 128）
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return NULL;
}

// 64-bits 变长编码整型值解码
bool GetVarint64(Slice* input, uint64_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = GetVarint64Ptr(p, limit, value);
  if (q == NULL) {
    return false;
  } else {
    *input = Slice(q, limit - q);
    return true;
  }
}

// 解析带 "变长长度" 头部的字符串 p~limit（编码查看 PutLengthPrefixedSlice），存
// 至 result 结果（已删除长度字段）
// | varint(slice len) | slice |
const char* GetLengthPrefixedSlice(const char* p, const char* limit,
                                   Slice* result) {
  uint32_t len;
  p = GetVarint32Ptr(p, limit, &len);
  if (p == NULL) return NULL;
  if (p + len > limit) return NULL;
  *result = Slice(p, len);
  return p + len;
}

// 解析带 "变长长度" 头部的字符串切片 input（编码查看 PutLengthPrefixedSlice），存
// 至 result 结果（已删除长度字段）
// | varint(slice len) | slice |
bool GetLengthPrefixedSlice(Slice* input, Slice* result) {
  uint32_t len;
  // | varint(slice len) | slice |
  if (GetVarint32(input, &len) &&
      input->size() >= len) {
    // 返回被解析的字符串
    *result = Slice(input->data(), len);
    // 删除已解析字符串
    input->remove_prefix(len);
    return true;
  } else {
    return false;
  }
}

}  // namespace leveldb
