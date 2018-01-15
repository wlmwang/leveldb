// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
#include "port/port.h"
#include "leveldb/status.h"

namespace leveldb {

// 拷贝底层 state_ 数据
const char* Status::CopyState(const char* state) {
  uint32_t size;
  // 获取 message 长度，则 state 长度为 size+5
  memcpy(&size, state, sizeof(size));
  char* result = new char[size + 5];
  // 拷贝所有 state_ 字节数据
  memcpy(result, state, size + 5);
  return result;
}

// 根据 msg+ ": " + msg2 字符串，构造 Status 对象
Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  // 成功状态无需构造 Status
  assert(code != kOk);
  const uint32_t len1 = msg.size();
  const uint32_t len2 = msg2.size();
  // 总长度为：msg+ ": " + msg2
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char* result = new char[size + 5];
  // 填充长度
  memcpy(result, &size, sizeof(size));
  // 填充错误码
  result[4] = static_cast<char>(code);
  // 拷贝 msg
  memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    // 拷贝 msg2
    memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::ToString() const {
  if (state_ == NULL) {
    return "OK";
  } else {
    char tmp[30];
    const char* type;
    switch (code()) {
      case kOk:
        type = "OK";
        break;
      case kNotFound:
        type = "NotFound: ";
        break;
      case kCorruption:
        type = "Corruption: ";
        break;
      case kNotSupported:
        type = "Not implemented: ";
        break;
      case kInvalidArgument:
        type = "Invalid argument: ";
        break;
      case kIOError:
        type = "IO error: ";
        break;
      default:
        snprintf(tmp, sizeof(tmp), "Unknown code(%d): ",
                 static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    // 获取长度
    uint32_t length;
    memcpy(&length, state_, sizeof(length));
    // 拷贝错误字符串
    result.append(state_ + 5, length);
    return result;
  }
}

}  // namespace leveldb
