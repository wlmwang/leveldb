// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Logger implementation that can be shared by all environments
// where enough posix functionality is available.

#ifndef STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
#define STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_

#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "leveldb/env.h"

namespace leveldb {

// @tips
// typedef char *va_list
// #define _INTSIZEOF(n)  ((sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1))
// 
// 得到可变参数中第一个参数的首地址 
// #define va_start(ap,v) (ap = (va_list)&v + _INTSIZEOF(v))
// 
// 将参数转换成需要的类型，并使ap指向下一个参数
// #define va_arg(ap,type)  (*(type *)((ap += _INTSIZEOF(type)) - _INTSIZEOF(type)))
// 
// #define va_end(ap) (ap = (va_list)0)
// 
// 函数参数保存到栈中，根据 va_start(ap,v) 宏定义，可以看出此函数获得可变参数受地址，接着通
// 过 va_arg 取得各可变参数，最后调用 va_end 函数把 ap 指向空

// POSIX 平台日志实现
class PosixLogger : public Logger {
 private:
  // 文件句柄
  FILE* file_;
  // 当前线程 tid
  uint64_t (*gettid_)();  // Return the thread id for the current thread
 public:
  PosixLogger(FILE* f, uint64_t (*gettid)()) : file_(f), gettid_(gettid) { }
  virtual ~PosixLogger() {
    // 关闭日志句柄
    fclose(file_);
  }
  // 记录日志字符串实现方法
  virtual void Logv(const char* format, va_list ap) {
    // 获取线程 tid
    const uint64_t thread_id = (*gettid_)();

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    // 
    // 尝试两次：第一次使用固定大小的栈分配缓冲区，第二次使用更大的动态分配缓冲区
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        // 栈空间
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        // 堆空间
        bufsize = 30000;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize; // 超尾指针

      // 当前时间转换成字符串
      struct timeval now_tv;
      gettimeofday(&now_tv, NULL);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p,
                    "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id));

      // Print the message
      // 
      // 整理日志信息字符串，可变参数
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        // @tips
        // \file <stdarg.h>
        // int vsnprintf(char *str, size_t size, const char *format, va_list ap);
        // 将可变参数 ap 按格式化字符串 format 输出到一个字符数组 str 中。size 为 str 可
        // 接受的最大字节数，防止产生数组越界
        // 
        // 执行成功，返回写入到字符数组 str 中的字符个数（不包含终止符），最大不超过 size；执
        // 行失败，返回负值，并置 errno
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // Truncate to available space if necessary
      // 
      // 如有必要，截断可用空间
      if (p >= limit) {
        if (iter == 0) {
          // 用较大的堆缓冲区再试一次
          continue;       // Try again with larger buffer
        } else {
          // 截断信息，存放 \n
          p = limit - 1;
        }
      }

      // Add newline if necessary
      // 
      // 如有必要添加换行符
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      // 越界退出
      assert(p <= limit);
      // 写入日志文件
      fwrite(base, 1, p - base, file_);
      // 刷新日志文件 I/O 缓冲区到磁盘
      fflush(file_);
      if (base != buffer) {
        // 堆上分配空间，释放（栈/堆地址，在 POSIX 平台上"逻辑地址"是隔离的）
        delete[] base;
      }
      break;
    }
  }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
