// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
// 
// 切片是一个简单的结构，包含一个指向外部存储器和大小的指针。Slice 的用户必须确保在
// 释放相应的外部存储器之后不再使用该切片
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.
// 
// 多个线程可以在没有外部同步的情况下在 Slice 上调用 const 方法，但是如果任何线程
// 可能调用非 const 方法，则访问相同 Slice 的所有线程都必须使用外部同步

#ifndef STORAGE_LEVELDB_INCLUDE_SLICE_H_
#define STORAGE_LEVELDB_INCLUDE_SLICE_H_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <string>

namespace leveldb {

// 字符串切片
// 相比 std::string, Slice 可手动管理 data_ 内存，但实际上 Slice 并不真正
// 管理 data_ 内存的分配与释放
class Slice {
 public:
  // Create an empty slice.
  // 
  // 创建空字符串切片
  Slice() : data_(""), size_(0) { }

  // Create a slice that refers to d[0,n-1].
  // 
  // 创建字符串切片。底层 _data 指针指向 d[0,n-1]
  Slice(const char* d, size_t n) : data_(d), size_(n) { }

  // Create a slice that refers to the contents of "s"
  // 
  // 创建字符串切片。底层 _data 指针指向 s 的内容
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

  // Create a slice that refers to s[0,strlen(s)-1]
  // 
  // 创建一个指向 s[0,strlen(s)-1] 的切片
  Slice(const char* s) : data_(s), size_(strlen(s)) { }

  // Return a pointer to the beginning of the referenced data
  // 
  // 返回一个指向引用 data_ 开始的指针
  const char* data() const { return data_; }

  // Return the length (in bytes) of the referenced data
  // 
  // 返回引用 data_ 的长度（以字节为单位）
  size_t size() const { return size_; }

  // Return true iff the length of the referenced data is zero
  // 
  // 如果引用字符串未空，返回 true
  bool empty() const { return size_ == 0; }

  // Return the ith byte in the referenced data.
  // REQUIRES: n < size()
  // 
  // 返回引用 data_ 中的第 n 个字节
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  // Change this slice to refer to an empty array
  // 
  // 改变这个切片，来引用一个空的数组
  void clear() { data_ = ""; size_ = 0; }

  // Drop the first "n" bytes from this slice.
  // 
  // 从这个切片中删除首起始的 "n" 个字节数据
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  // Return a string that contains the copy of the referenced data.
  // 
  // 返回一个包含引用 data_ 的字符串副本对象
  std::string ToString() const { return std::string(data_, size_); }

  // Three-way comparison.  Returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  //   
  // 三路比较（底层使用字符串比较函数 memcmp）
  int compare(const Slice& b) const;

  // Return true iff "x" is a prefix of "*this"
  // 
  // 如果 "x" 是 "*this" 的前缀，则返回 true
  bool starts_with(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_, x.data_, x.size_) == 0));
  }

 private:
  const char* data_;
  size_t size_;

  // Intentionally copyable
};

// 切片全局 == 运算符重载
inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) {
  return !(x == y);
}

// 切片 "三路比较" 方法
inline int Slice::compare(const Slice& b) const {
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

}  // namespace leveldb


#endif  // STORAGE_LEVELDB_INCLUDE_SLICE_H_
