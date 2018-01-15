// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_posix.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include "util/logging.h"

namespace leveldb {
namespace port {

// pthread 线程函数调用统一校验函数。凡是返回非 0 都将 abort() 程序
static void PthreadCall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    abort();
  }
}

// 初始化互斥体
Mutex::Mutex() { PthreadCall("init mutex", pthread_mutex_init(&mu_, NULL)); }

// 销毁互斥体
Mutex::~Mutex() { PthreadCall("destroy mutex", pthread_mutex_destroy(&mu_)); }

// 互斥体上锁
void Mutex::Lock() { PthreadCall("lock", pthread_mutex_lock(&mu_)); }

// 互斥体解锁
void Mutex::Unlock() { PthreadCall("unlock", pthread_mutex_unlock(&mu_)); }

CondVar::CondVar(Mutex* mu)
    : mu_(mu) {
    // 初始化条件变量
    PthreadCall("init cv", pthread_cond_init(&cv_, NULL));
}

// 销毁条件变量
CondVar::~CondVar() { PthreadCall("destroy cv", pthread_cond_destroy(&cv_)); }

// 阻塞等待条件变量事件（基于互斥体 mu_ ）
void CondVar::Wait() {
  PthreadCall("wait", pthread_cond_wait(&cv_, &mu_->mu_));
}

// 随机通知一个在该条件变量上被阻塞的线程
void CondVar::Signal() {
  PthreadCall("signal", pthread_cond_signal(&cv_));
}

// 通知所有在该条件变量上被阻塞的线程
void CondVar::SignalAll() {
  PthreadCall("broadcast", pthread_cond_broadcast(&cv_));
}

// 线程安全初始化函数
void InitOnce(OnceType* once, void (*initializer)()) {
  PthreadCall("once", pthread_once(once, initializer));
}

}  // namespace port
}  // namespace leveldb
