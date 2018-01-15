// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "leveldb/cache.h"
#include "port/port.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {

Cache::~Cache() {
}

namespace {

// LRU cache implementation
// 
// LRU cache 的实现类

// An entry is a variable length heap-allocated structure.  Entries
// are kept in a circular doubly linked list ordered by access time.
// 
// 一个可变长度、由堆分配缓存中的实体结构（元素）。实体按照访问时间排列在一个
// 循环双向链表中
struct LRUHandle {
  // 值
  void* value;
  // 从 cache 清除实体时，key/value 的删除器
  void (*deleter)(const Slice&, void* value);
  // 映射到同一个桶的实体下一个指针（拉链法解决 HandleTable 冲突指针）
  LRUHandle* next_hash;
  // 双链表前/后指针
  // 从 LRUCache::lru_.prev 迭代为最近使用，从 LRUCache::lru_.next 迭代为
  // 最近未使用
  LRUHandle* next;
  LRUHandle* prev;
  // 占用 cache 大小
  size_t charge;      // TODO(opt): Only allow uint32_t?
  // key 长度。恢复采用 "柔性数组" 的 key_data
  size_t key_length;
  // 引用计数器
  uint32_t refs;
  // key 哈希值。用于快速分片和比较
  uint32_t hash;      // Hash of key(); used for fast sharding and comparisons
  // 键。采用 "柔性数组" 设计技巧
  char key_data[1];   // Beginning of key

  // 返回该实体 key 的字符切片
  Slice key() const {
    // For cheaper lookups, we allow a temporary Handle object
    // to store a pointer to a key in "value".
    // 
    // 允许一个临时的 Handle 对象，在 "value" 中存储 key 的指针
    // 获取该 Handle 的 key 字符串切片
    if (next == this) {
      return *(reinterpret_cast<Slice*>(value));
    } else {
      return Slice(key_data, key_length);
    }
  }
};

// We provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  E.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
// 
// 提供了一个简单哈希表。它消除了大量的移植问题，而且比我们测试过的一些编译器/运行
// 时组合中的某些内置哈希表实现要快。例如，readrandom 比 g++4.4.3 的内置哈希表
// 加速了 5％
class HandleTable {
 public:
  HandleTable() : length_(0), elems_(0), list_(NULL) { Resize(); }
  ~HandleTable() { delete[] list_; }

  // 返回指向与 key/hash 匹配的缓存实体的指针。如果没有这样的缓存条目，则返回
  // 一个指向相应链表中尾部槽的指针 NULL
  LRUHandle* Lookup(const Slice& key, uint32_t hash) {
    return *FindPointer(key, hash);
  }

  // 添加/替换一个实体到 HandleTable 中，返回旧指针（用以释放该无用节点）
  LRUHandle* Insert(LRUHandle* h) {
    LRUHandle** ptr = FindPointer(h->key(), h->hash);
    LRUHandle* old = *ptr;
    // 查无此 key，新实体插入映射的桶的链表的头部，否则删除原始实体后再添加新实体
    h->next_hash = (old == NULL ? NULL : old->next_hash);
    *ptr = h;
    // old==NULL 表明实体是一个新元素
    if (old == NULL) {
      ++elems_; // 自增实体个数
      if (elems_ > length_) {
        // Since each cache entry is fairly large, we aim for a small
        // average linked list length (<= 1).
        // 
        // 当实体数量超过桶的数量，做 rehash 操作，做到平均链表长度 (<= 1)，
        // 以尽量减少碰撞
        Resize();
      }
    }
    return old;
  }

  // 删除 key/hash 对应实体
  LRUHandle* Remove(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = FindPointer(key, hash);
    LRUHandle* result = *ptr;
    if (result != NULL) {
      // 删除查找到的 key（即，跳过该实体，并递减实体数量）
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  // The table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
  // 
  // 该表由一个存储桶数组组成，其中每个存储桶是散列到存储桶中的缓存条目的链接列表
  uint32_t length_; // 桶容量（2 的幂次方）
  uint32_t elems_;  // 实体数
  LRUHandle** list_;  // 桶数组（指针数组）

  // Return a pointer to slot that points to a cache entry that
  // matches key/hash.  If there is no such cache entry, return a
  // pointer to the trailing slot in the corresponding linked list.
  // 
  // 返回与 key/hash 缓存项匹配的 "前一个节点的 next_hash(下一个节点) 指针" 的指针
  // 如果没有这样的缓存条目，则返回 HandleTable 指向相应映射桶指针的指针
  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
    // HandleTable 指向相应映射桶指针的指针
    LRUHandle** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != NULL &&
           ((*ptr)->hash != hash || key != (*ptr)->key())) {
      // "前一个节点的 next_hash(下一个节点) 指针" 的指针
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  // 调整 HandleTable 大小以降低桶冲突（rehash）
  void Resize() {
    // 桶容量最小为 4 个
    uint32_t new_length = 4;
    // 将大小调整为初始容量的最低 2 的幂次方
    while (new_length < elems_) {
      new_length *= 2;
    }
    // 分配桶数组内存
    LRUHandle** new_list = new LRUHandle*[new_length];
    // 桶数组（指针数组）清 0
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    // 重新映射所有实体（rehash）
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle* h = list_[i];
      while (h != NULL) {
        // 处理映射到同一个桶的所有实体
        LRUHandle* next = h->next_hash;
        uint32_t hash = h->hash;
        // 映射到新的 HandleTable 中
        LRUHandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    // 处理完所有实体校验
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

// A single shard of sharded cache.
// 
// 共享缓存类（ShardedLRUCache）的单个分片。
class LRUCache {
 public:
  LRUCache();
  ~LRUCache();

  // Separate from constructor so caller can easily make an array of LRUCache
  // 
  // 与构造函数分开，所以调用者可以轻松地创建一个 LRUCache 数组
  // 设置 Cache 容量
  void SetCapacity(size_t capacity) { capacity_ = capacity; }

  // Like Cache methods, but with an extra "hash" parameter.
  // 
  // 像 Cache 方法一样，但有一个额外的 "hash" 参数
  // 插入/查找/释放/删除 key/value 缓存项，并指定删除器
  Cache::Handle* Insert(const Slice& key, uint32_t hash,
                        void* value, size_t charge,
                        void (*deleter)(const Slice& key, void* value));
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);
  void Release(Cache::Handle* handle);
  void Erase(const Slice& key, uint32_t hash);

 private:
  // LRU 控制实现函数

  // 删除实体 e 节点
  void LRU_Remove(LRUHandle* e);
  // 添加实体 e 节点。并使之成为最新节点
  // 即，从 lru_.prev 迭代为最近使用，从 lru_.next 迭代为最近未使用
  void LRU_Append(LRUHandle* e);
  // 释放引用计数（若引用计数器为 0，释放内存）
  void Unref(LRUHandle* e);

  // Initialized before use.
  // 
  // Cache 容量阈值（usage_ 不能超过该值）。使用前初始化
  size_t capacity_;

  // mutex_ protects the following state.
  // 
  // 互斥锁。保护以下状态
  port::Mutex mutex_;
  // 全部 LRUHandle 缓存项内存字节数总和
  size_t usage_;

  // Dummy head of LRU list.
  // lru.prev is newest entry, lru.next is oldest entry.
  // 
  // LRU 链表的头。组成 LRU 的双向链表
  LRUHandle lru_;

  // LRUCache 缓存的所有缓存实体头
  HandleTable table_;
};

LRUCache::LRUCache()
    : usage_(0) {
  // Make empty circular linked list
  // 
  // 创建空的循环链表
  lru_.next = &lru_;
  lru_.prev = &lru_;
}

LRUCache::~LRUCache() {
  // 清除所有缓存实体
  for (LRUHandle* e = lru_.next; e != &lru_; ) {
    LRUHandle* next = e->next;
    // 如果调用者有一个未释放的句柄，即报错
    assert(e->refs == 1);  // Error if caller has an unreleased handle
    // 释放引用计数（若引用计数器为 0，释放内存）
    Unref(e);
    e = next;
  }
}

// 释放引用计数（若引用计数器为 0，释放内存）
void LRUCache::Unref(LRUHandle* e) {
  // 引用计数器不可能为 0
  assert(e->refs > 0);
  e->refs--;
  // 引用计数为 0，释放内存
  if (e->refs <= 0) {
    // 减去响应内存字节数大小
    usage_ -= e->charge;
    // 调用 key/value 删除器
    (*e->deleter)(e->key(), e->value);
    // 释放 LRUHandle 实体内存
    free(e);
  }
}

// 从 lru 中删除实体 e 节点
void LRUCache::LRU_Remove(LRUHandle* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

// 添加实体 e 节点到 lru 中。并使之成为最新节点
// 即，从 lru_.prev 迭代为最近使用，从 lru_.next 迭代为最近未使用
void LRUCache::LRU_Append(LRUHandle* e) {
  // Make "e" newest entry by inserting just before lru_
  // 
  // 通过在 lru_ 之前插入 "e" 来创建最新的条目
  e->next = &lru_;
  e->prev = lru_.prev;
  e->prev->next = e;
  e->next->prev = e;
}

// 查找 key/hash 缓存实体
Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  LRUHandle* e = table_.Lookup(key, hash);
  if (e != NULL) {
    // 自增引用计数器
    e->refs++;
    // 删除节点，并添加到链表头以表示该实体节点最新访问
    LRU_Remove(e);
    LRU_Append(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

// 释放节点
void LRUCache::Release(Cache::Handle* handle) {
  MutexLock l(&mutex_);
  // 释放引用计数（若引用计数器为 0，释放内存）
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

// 插入 key/value 缓存项，并指定删除器
Cache::Handle* LRUCache::Insert(
    const Slice& key, uint32_t hash, void* value, size_t charge,
    void (*deleter)(const Slice& key, void* value)) {
  MutexLock l(&mutex_);

  // 申请分配缓存实体节点内存
  LRUHandle* e = reinterpret_cast<LRUHandle*>(
      malloc(sizeof(LRUHandle)-1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  // 一个来自 LRUCache 引用，一个是返回句柄
  e->refs = 2;  // One from LRUCache, one for the returned handle
  // 复制 key 副本
  memcpy(e->key_data, key.data(), key.size());
  // 添加节点实体到 lru 中
  LRU_Append(e);
  // 增加 Cache 字节大小
  usage_ += charge;

  // 添加实体到 table 中
  LRUHandle* old = table_.Insert(e);
  if (old != NULL) {  // 节点更新（添加相同的 key）
    // 删除旧节点
    LRU_Remove(old);
    Unref(old);
  }

  // usage_ 不能超过容量阈值
  while (usage_ > capacity_ && lru_.next != &lru_) {
    // 从 lru_.next 遍历，删除最近最少使用的节点
    LRUHandle* old = lru_.next;
    LRU_Remove(old);
    table_.Remove(old->key(), old->hash);
    Unref(old);
  }

  return reinterpret_cast<Cache::Handle*>(e);
}

// 删除 key/hash 缓存项，并指定删除器
void LRUCache::Erase(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  LRUHandle* e = table_.Remove(key, hash);
  if (e != NULL) {
    // 删除节点
    LRU_Remove(e);
    // 释放引用计数（若引用计数器为 0，释放内存）
    Unref(e);
  }
}

// 最大 LRUCache 分片
static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

// 共享缓存类( Cache 实现内)
class ShardedLRUCache : public Cache {
 private:
  LRUCache shard_[kNumShards];
  // 互斥锁。保护 last_id_ 值
  port::Mutex id_mutex_;
  uint64_t last_id_;

  // key 字符切片哈希值
  static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }

  // 映射到哪个切片上
  static uint32_t Shard(uint32_t hash) {
    return hash >> (32 - kNumShardBits);
  }

 public:
  explicit ShardedLRUCache(size_t capacity)
      : last_id_(0) {
    // 所有切片总容量最小要大于 capacity 的最小整数倍
    const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].SetCapacity(per_shard);
    }
  }
  virtual ~ShardedLRUCache() { }
  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }
  virtual Handle* Lookup(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);
  }
  virtual void Release(Handle* handle) {
    LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);
    shard_[Shard(h->hash)].Release(handle);
  }
  virtual void Erase(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }
  virtual void* Value(Handle* handle) {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }
  // 返回最小 id 值
  virtual uint64_t NewId() {
    MutexLock l(&id_mutex_);
    return ++(last_id_);
  }
};

}  // end anonymous namespace

// 创建一个具有固定大小容量的新缓存 ShardedLRUCache
Cache* NewLRUCache(size_t capacity) {
  return new ShardedLRUCache(capacity);
}

}  // namespace leveldb
