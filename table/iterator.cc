// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/iterator.h"

namespace leveldb {

Iterator::Iterator() {
  cleanup_.function = NULL;
  cleanup_.next = NULL;
}

// 运行清理函数链表中每个节点的清理函数 function(arg1,arg2)
Iterator::~Iterator() {
  if (cleanup_.function != NULL) {
    // 调用清理函数（头节点）
    (*cleanup_.function)(cleanup_.arg1, cleanup_.arg2);
    // 依次访问链表节点
    for (Cleanup* c = cleanup_.next; c != NULL; ) {
      // 调用清理函数
      (*c->function)(c->arg1, c->arg2);
      Cleanup* next = c->next;
      // 释放节点
      delete c;
      c = next;
    }
  }
}

// 注册清理函数
void Iterator::RegisterCleanup(CleanupFunction func, void* arg1, void* arg2) {
  assert(func != NULL);
  Cleanup* c;
  if (cleanup_.function == NULL) {
    // 头节点
    c = &cleanup_;
  } else {
    // 生成新的节点，并插入表头
    c = new Cleanup;
    c->next = cleanup_.next;
    cleanup_.next = c;
  }
  c->function = func;
  c->arg1 = arg1;
  c->arg2 = arg2;
}

namespace {
// 空迭代器
class EmptyIterator : public Iterator {
 public:
  EmptyIterator(const Status& s) : status_(s) { }
  virtual bool Valid() const { return false; }
  virtual void Seek(const Slice& target) { }
  virtual void SeekToFirst() { }
  virtual void SeekToLast() { }
  virtual void Next() { assert(false); }
  virtual void Prev() { assert(false); }
  Slice key() const { assert(false); return Slice(); }
  Slice value() const { assert(false); return Slice(); }
  virtual Status status() const { return status_; }
 private:
  Status status_;
};
}  // namespace

// 创建空迭代器
Iterator* NewEmptyIterator() {
  return new EmptyIterator(Status::OK());
}

// 创建带有错误状态的迭代器
Iterator* NewErrorIterator(const Status& status) {
  return new EmptyIterator(status);
}

}  // namespace leveldb
