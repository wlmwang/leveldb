// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/env.h"

namespace leveldb {

Env::~Env() {
}

SequentialFile::~SequentialFile() {
}

RandomAccessFile::~RandomAccessFile() {
}

WritableFile::~WritableFile() {
}

Logger::~Logger() {
}

FileLock::~FileLock() {
}

// 记录日志字符串信息
void Log(Logger* info_log, const char* format, ...) {
  if (info_log != NULL) {
    va_list ap;
    va_start(ap, format);
    info_log->Logv(format, ap);
    va_end(ap);
  }
}

// 字符串写入文件底层函数
static Status DoWriteStringToFile(Env* env, const Slice& data,
                                  const std::string& fname,
                                  bool should_sync) {
  // 写入方式打开文件
  WritableFile* file;
  Status s = env->NewWritableFile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  // 写入字符串
  s = file->Append(data);
  if (s.ok() && should_sync) {
    // 同步刷新 I/O 缓冲区
    // 底层调用 fdatasync 库函数
    s = file->Sync();
  }
  if (s.ok()) {
    // 关闭文件句柄
    s = file->Close();
  }
  // 析构时也会自动关闭该文件句柄
  delete file;  // Will auto-close if we did not close above
  if (!s.ok()) {
    // 字符串写入失败，删除该文件
    env->DeleteFile(fname);
  }
  return s;
}

// 将 "data" 写入指定的文件(不实时刷新 I/O 缓冲区，由 OS 自身决定刷新时机)
Status WriteStringToFile(Env* env, const Slice& data,
                         const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, false);
}

// 将 "data" 写入指定的文件(实时刷新 I/O 缓冲区)
Status WriteStringToFileSync(Env* env, const Slice& data,
                             const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, true);
}

// 将指定文件的内容读入 *data
Status ReadFileToString(Env* env, const std::string& fname, std::string* data) {
  data->clear();
  // 顺序读取方式打开文件
  SequentialFile* file;
  Status s = env->NewSequentialFile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  // 单次读取文件长度
  static const int kBufferSize = 8192;
  char* space = new char[kBufferSize];
  while (true) {
    Slice fragment;
    s = file->Read(kBufferSize, &fragment, space);
    if (!s.ok()) {
      break;
    }
    data->append(fragment.data(), fragment.size());
    if (fragment.empty()) {
      break;
    }
  }
  // 释放 space/file 内存，并 SequentialFile 在析构时关闭文件
  delete[] space;
  delete file;
  return s;
}

EnvWrapper::~EnvWrapper() {
}

}  // namespace leveldb
