// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"
#include <assert.h>

namespace leveldb {

// 内存块大小（4k 页）
static const int kBlockSize = 4096;

Arena::Arena() {
  blocks_memory_ = 0;
  // 第一次分配将分配一个块
  alloc_ptr_ = NULL;  // First allocation will allocate a block
  alloc_bytes_remaining_ = 0;
}

Arena::~Arena() {
  // 释放所有 new[] 分配的内存块
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

// 内存块不够分配 bytes 字节时，重新分配。可能直接向 OS 申请，或者重新分配新的内存块
char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    // Object is more than a quarter of our block size.  Allocate it separately
    // to avoid wasting too much space in leftover bytes.
    // 
    // 预分配的对象是块大小的四分之一。则，分开分配，以避免浪费剩余字节中的太多空间
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // We waste the remaining space in the current block.
  // 
  // 我们浪费掉（无视）当前块的还剩余的空间
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  // 分配 "bytes" 个字节的内存指针给客户端
  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

// 用 malloc 提供的对齐方式来分配内存
char* Arena::AllocateAligned(size_t bytes) {
  // 按指针大小对齐。最小为 8 字节
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  // 对齐大小（指针大小）应该是 2 的幂次
  assert((align & (align-1)) == 0);   // Pointer size should be a power of 2
  // 指针对齐计算。其实就是补齐指针不对齐的空位字节
  // 
  // @TODO
  // size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) % align;
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);
  size_t slop = (current_mod == 0 ? 0 : align - current_mod); // 空位大小
  size_t needed = bytes + slop; // 实际分配字节大小
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop; // 跳过空位
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // AllocateFallback always returned aligned memory
    // 
    // AllocateFallback 总是返回对齐的内存
    result = AllocateFallback(bytes);
  }
  // 再次检验对齐
  assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);
  return result;
}

// 向 OS 申请分配新的 block_bytes 大小内存，并加入内存块分配队列
char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_memory_ += block_bytes;
  // 加入内存块分配队列
  blocks_.push_back(result);
  return result;
}

}  // namespace leveldb
