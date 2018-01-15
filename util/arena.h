// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <vector>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

namespace leveldb {

// 简易内存池。做到小块内存申请不总是向 OS 发出调用，并统一释放内存
class Arena {
 public:
  Arena();
  ~Arena();

  // Return a pointer to a newly allocated memory block of "bytes" bytes.
  // 
  // 返回指向新分配的 "bytes" 个字节的内存指针给客户端
  char* Allocate(size_t bytes);

  // Allocate memory with the normal alignment guarantees provided by malloc
  // 
  // 用 malloc 提供的对齐方式来分配内存
  char* AllocateAligned(size_t bytes);

  // Returns an estimate of the total memory usage of data allocated
  // by the arena (including space allocated but not yet used for user
  // allocations).
  // 
  // 返回由 Arena 分配的总内存使用情况的估计值（包括已分配但尚未用于用户分配的空间）
  size_t MemoryUsage() const {
    return blocks_memory_ + blocks_.capacity() * sizeof(char*);
  }

 private:
  // 内存块不够分配 bytes 字节时，重新分配内存。可能单独向 OS 申请并直接返回，或者先重
  // 新分配新的内存块覆盖当前分配块，然后在分配内存返回
  char* AllocateFallback(size_t bytes);
  // 向 OS 申请分配新的 block_bytes 大小内存，并加入内存块分配队列
  char* AllocateNewBlock(size_t block_bytes);

  // Allocation state
  char* alloc_ptr_;   // 当前已分配内存块指针的位置
  size_t alloc_bytes_remaining_;  // 该内存块剩余大小

  // Array of new[] allocated memory blocks
  // 
  // new[] 分配的内存块数组
  std::vector<char*> blocks_;

  // Bytes of memory in blocks allocated so far
  size_t blocks_memory_;

  // No copying allowed
  Arena(const Arena&);
  void operator=(const Arena&);
};

// 返回指向新分配的 "bytes" 个字节的内存指针给客户端
inline char* Arena::Allocate(size_t bytes) {
  // The semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  // 
  // 如果我们允许 0 字节的分配，那么返回的语义有点混乱，所以我们在这里不允许它们
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    // 分配 "bytes" 个字节的内存指针给客户端
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  // 重新分配内存
  return AllocateFallback(bytes);
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
