// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// See port_example.h for documentation for the following types/functions.

#ifndef STORAGE_LEVELDB_PORT_PORT_POSIX_H_
#define STORAGE_LEVELDB_PORT_PORT_POSIX_H_

// 检测平台是否小端序
// 
// @tips
// MacOSX Darwin Kernel Version 17.3.0 x86_64 Intel(R) Core(TM) i5-4278U CPU
// \file <machine/endian.h>
// \file <i386/endian.h>
// #define __DARWIN_LITTLE_ENDIAN 1234
// #define __DARWIN_BYTE_ORDER __DARWIN_LITTLE_ENDIAN
#undef PLATFORM_IS_LITTLE_ENDIAN
#if defined(OS_MACOSX)
  #include <machine/endian.h>
  #if defined(__DARWIN_LITTLE_ENDIAN) && defined(__DARWIN_BYTE_ORDER)
    #define PLATFORM_IS_LITTLE_ENDIAN \
        (__DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN)
  #endif
#elif defined(OS_SOLARIS)
  #include <sys/isa_defs.h>
  #ifdef _LITTLE_ENDIAN
    #define PLATFORM_IS_LITTLE_ENDIAN true
  #else
    #define PLATFORM_IS_LITTLE_ENDIAN false
  #endif
#elif defined(OS_FREEBSD) || defined(OS_OPENBSD) ||\
      defined(OS_NETBSD) || defined(OS_DRAGONFLYBSD)
  #include <sys/types.h>
  #include <sys/endian.h>
  #define PLATFORM_IS_LITTLE_ENDIAN (_BYTE_ORDER == _LITTLE_ENDIAN)
#elif defined(OS_HPUX)
  #define PLATFORM_IS_LITTLE_ENDIAN false
#elif defined(OS_ANDROID)
  // Due to a bug in the NDK x86 <sys/endian.h> definition,
  // _BYTE_ORDER must be used instead of __BYTE_ORDER on Android.
  // See http://code.google.com/p/android/issues/detail?id=39824
  #include <endian.h>
  #define PLATFORM_IS_LITTLE_ENDIAN  (_BYTE_ORDER == _LITTLE_ENDIAN)
#else
  #include <endian.h>
#endif

#include <pthread.h>
#ifdef SNAPPY
#include <snappy.h>
#endif
#include <stdint.h>
#include <string>
#include "port/atomic_pointer.h"

// @tips
// Linux 2.6.32 x86_64 Intel(R) Xeon(R) CPU
// \file <endian.h>
// #define __LITTLE_ENDIAN 1234
// #define BYTE_ORDER __BYTE_ORDER
// 
// \file <bits/endian.h>
// #define __BYTE_ORDER __LITTLE_ENDIAN
#ifndef PLATFORM_IS_LITTLE_ENDIAN
#define PLATFORM_IS_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#endif

#if defined(OS_MACOSX) || defined(OS_SOLARIS) || defined(OS_FREEBSD) ||\
    defined(OS_NETBSD) || defined(OS_OPENBSD) || defined(OS_DRAGONFLYBSD) ||\
    defined(OS_ANDROID) || defined(OS_HPUX) || defined(CYGWIN)
// Use fread/fwrite/fflush on platforms without _unlocked variants
// 
// 在没有 _unlocked 变体的平台上使用 fread/fwrite/fflush
// 
// @tips
// \file <stdio.h>
// size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
// 函数每次从 stream 中最多读取 count 个单元，每个单元大小为 size 个字节的数据。
// 将读取的数据放到 buffer。文件流的位置指针后移 size*count 字节
// 
// 返回实际读取的单元个数。如果小于 count，则可能文件结束或读取出错。可以用 ferror() 检测
// 是否读取出错，用 feof() 函数检测是否到达文件结尾。如果 size 或 count 为 0，则返回 0
#define fread_unlocked fread
// @tips
// \file <stdio.h>
// size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
// 函数每次向 stream 中写入 count 个单元，每个单元大小为 size 个字节数据。文件流的位置指针
// 后移 size*count 字节。
// 
// 返回成功写入的单元个数。如果小于 count，则说明发生了错误，文件流错误标志位将被设置，随后可
// 以通过 ferror() 函数判断。如果 size 或 count 的值为 0，则返回值为 0，并且文件流的位置指
// 针保持不变
#define fwrite_unlocked fwrite
// @tips
// \file <stdio.h>
// int fflush(FILE* stream);
// 函数会强迫将缓冲区内的数据写回参数 stream 指定的文件中。如果参数 stream 为 NULL，fflush()
// 会将所有打开的文件数据更新。fflush() 也可用于标准输入（stdin）和标准输出（stdout），用来清
// 空标准输入输出缓冲区
// 
// 成功返回 0，失败返回 EOF，错误代码存于 errno 中。指定的流没有缓冲区或者只读打开时也返回 0 值
#define fflush_unlocked fflush
#endif

#if defined(OS_MACOSX) || defined(OS_FREEBSD) ||\
    defined(OS_OPENBSD) || defined(OS_DRAGONFLYBSD)
// Use fsync() on platforms without fdatasync()
#define fdatasync fsync
#endif

#if defined(OS_ANDROID) && __ANDROID_API__ < 9
// fdatasync() was only introduced in API level 9 on Android. Use fsync()
// when targetting older platforms.
#define fdatasync fsync
#endif

namespace leveldb {
namespace port {

// 小端序标志位
static const bool kLittleEndian = PLATFORM_IS_LITTLE_ENDIAN;
#undef PLATFORM_IS_LITTLE_ENDIAN

class CondVar;

// POSIX 平台互斥体实现
class Mutex {
 public:
  Mutex();
  ~Mutex();

  void Lock();
  void Unlock();
  void AssertHeld() { }

 private:
  friend class CondVar;
  // 互斥锁实体
  pthread_mutex_t mu_;

  // No copying
  // 
  // 禁止 拷贝/赋值
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};

// POSIX 平台条件变量实现
class CondVar {
 public:
  explicit CondVar(Mutex* mu);
  ~CondVar();
  void Wait();
  void Signal();
  void SignalAll();
 private:
  // 条件变量实体
  pthread_cond_t cv_;
  // 互斥体对象指针
  Mutex* mu_;
};

// 线程安全初始化函数
// 
// @tips
// \file <pthread.h>
// int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))；
// 函数使用初值为 PTHREAD_ONCE_INIT 的 once_control 变量，保证 init_routine() 函数在本进程执行序列
// 中仅执行一次
// Linux 中使用互斥锁和条件变量保证由 pthread_once() 指定的函数执行且仅执行一次，而 once_control 表示
// 是否执行过
// 在 Linux 中，"一次性函数" 的执行状态有三种：NEVER(0)/IN_PROGRESS(1)/DONE(2)，如果 once_control
// 初值设为 1，则由于所有 pthread_once() 都必须等待其中一个激发 "已执行一次" 信号，即所有 pthread_once()
// 都会陷入永久的等待中；如果设为 2，则表示该函数已执行过一次，从而所有 pthread_once() 都会立即返回 0
typedef pthread_once_t OnceType;
#define LEVELDB_ONCE_INIT PTHREAD_ONCE_INIT
extern void InitOnce(OnceType* once, void (*initializer)());

// snappy 压缩指定长度字符数组 input 数据，输出至 std::string 类型的 output
inline bool Snappy_Compress(const char* input, size_t length,
                            ::std::string* output) {
#ifdef SNAPPY
  // 基于较低级别字符数组的 snappy 压缩函数
  // \file <snappy/snappy.h>
  // void RawCompress(const char* input,size_t input_length,char* compressed,size_t* compressed_length);
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif

  return false;
}

// snappy 获取压缩的字符数组 input 数据，被解压后最大长度，输出至 result
inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#ifdef SNAPPY
  // \file <snappy/snappy.h>
  // bool GetUncompressedLength(const char* compressed, size_t compressed_length, size_t* result);
  return snappy::GetUncompressedLength(input, length, result);
#else
  return false;
#endif
}

// snappy 解压指定长度字符数组 input 数据（已被压缩），输出至 char* 类型的 output（有足够缓冲区）
inline bool Snappy_Uncompress(const char* input, size_t length,
                              char* output) {
#ifdef SNAPPY
  // 基于较低级别字符数组的 snappy 解压函数
  // \file <snappy/snappy.h>
  // void RawUncompress(const char* compressed, size_t compressed_length, char* uncompressed);
  return snappy::RawUncompress(input, length, output);
#else
  return false;
#endif
}

// @TODO
// 堆分析
inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

} // namespace port
} // namespace leveldb

#endif  // STORAGE_LEVELDB_PORT_PORT_POSIX_H_
