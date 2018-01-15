// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
#define STORAGE_LEVELDB_INCLUDE_OPTIONS_H_

#include <stddef.h>

namespace leveldb {

class Cache;
class Comparator;
class Env;
class FilterPolicy;
class Logger;
class Snapshot;

// DB contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  Each block may be compressed before
// being stored in a file.  The following enum describes which
// compression method (if any) is used to compress a block.
// 
// 数据库内容存储在一组块中，每个块都包含一系列键值对。每个块可以在被存储在一个
// 文件之前被压缩。
// 
// 以下枚举了使用哪种压缩方法（如果有）来压缩块
enum CompressionType {
  // NOTE: do not change the values of existing entries, as these are
  // part of the persistent format on disk.
  // 
  // 注：不要更改现有条目的对应的压缩算法，因为它们是磁盘上持久格式的一部分
  kNoCompression     = 0x0,
  kSnappyCompression = 0x1
};

// Options to control the behavior of a database (passed to DB::Open)
// 
// 数据库行为的控制选项（传递给DB::Open）
struct Options {
  // -------------------
  // Parameters that affect behavior

  // Comparator used to define the order of keys in the table.
  // Default: a comparator that uses lexicographic byte-wise ordering
  //
  // REQUIRES: The client must ensure that the comparator supplied
  // here has the same name and orders keys *exactly* the same as the
  // comparator provided to previous open calls on the same DB.
  // 
  // 用于定义数据表（table）中键的顺序的比较器。默认：使用按字节顺序的比较器
  const Comparator* comparator;

  // If true, the database will be created if it is missing.
  // Default: false
  // 
  // If true, 如果db目录不存在，open时创建新目录
  bool create_if_missing;

  // If true, an error is raised if the database already exists.
  // Default: false
  // 
  // If true, 如果 db 目录存在，open 时报错
  bool error_if_exists;

  // If true, the implementation will do aggressive checking of the
  // data it is processing and will stop early if it detects any
  // errors.  This may have unforeseen ramifications: for example, a
  // corruption of one DB entry may cause a large number of entries to
  // become unreadable or for the entire DB to become unopenable.
  // Default: false
  // 
  // If true, 则实现将对正在处理的数据进行积极的检查，如果检测到任何错误，则将
  // 提前停止。这可能会有不可预见的后果：例如，一个数据库条目的损坏可能导致大量
  // 条目变得不可读，或使整个数据库变得无法打开 
  bool paranoid_checks;

  // Use the specified object to interact with the environment,
  // e.g. to read/write files, schedule background work, etc.
  // Default: Env::Default()
  // 
  // 使用指定的对象与平台环境交互，例如：读/写文件，安排后台工作等
  Env* env;

  // Any internal progress/error information generated by the db will
  // be written to info_log if it is non-NULL, or to a file stored
  // in the same directory as the DB contents if info_log is NULL.
  // Default: NULL
  // 
  // 由 DB 生成的任何内部进程/错误信息将被写入info_log（如果它是 non-NULL），
  // 或写入与 DB 内容存储在同一目录中的文件中（如果 info_log 为 NULL ）
  Logger* info_log;

  // -------------------
  // Parameters that affect performance

  // Amount of data to build up in memory (backed by an unsorted log
  // on disk) before converting to a sorted on-disk file.
  //
  // Larger values increase performance, especially during bulk loads.
  // Up to two write buffers may be held in memory at the same time,
  // so you may wish to adjust this parameter to control memory usage.
  // Also, a larger write buffer will result in a longer recovery time
  // the next time the database is opened.
  //
  // Default: 4MB
  // 
  // 在转换存储为分类的磁盘文件之前，在内存中建立的数据大小（由未排序的磁盘日志
  // 支持）
  // 
  // 较大的值会提高性能，尤其是在批量加载时。最多可同时在内存中保存两个写入缓冲
  // 区，因此您可能希望调整此参数以控制内存使用情况。此外，较大的写入缓冲区会在
  // 下次打开数据库时导致更长的恢复时间
  size_t write_buffer_size;

  // Number of open files that can be used by the DB.  You may need to
  // increase this if your database has a large working set (budget
  // one open file per 2MB of working set).
  //
  // Default: 1000
  // 
  // DB 可以使用打开文件的数量。如果你的数据库有一个大的工作集（每个 2MB 的工作
  // 集预算一个打开的文件），你可能需要增加这个值
  int max_open_files;

  // Control over blocks (user data is stored in a set of blocks, and
  // a block is the unit of reading from disk).

  // If non-NULL, use the specified cache for blocks.
  // If NULL, leveldb will automatically create and use an 8MB internal cache.
  // Default: NULL
  // 
  // 控制块（用户数据存储在一组块中，块是从磁盘读取的单位）
  //  
  // 如果 non-NULL，则为指定的块使用指定的缓存
  // 如果为 NULL，则 leveldb 将自动创建并使用一个 8MB 的内部缓存
  Cache* block_cache;

  // Approximate size of user data packed per block.  Note that the
  // block size specified here corresponds to uncompressed data.  The
  // actual size of the unit read from disk may be smaller if
  // compression is enabled.  This parameter can be changed dynamically.
  //
  // Default: 4K
  // 
  // 用户数据打包块的近视大小。请注意，此处指定的块大小对应于未压缩的数据。如果启用
  // 压缩，则从磁盘读取的单位的实际大小可能会更小。该参数可以动态更改
  size_t block_size;

  // Number of keys between restart points for delta encoding of keys.
  // This parameter can be changed dynamically.  Most clients should
  // leave this parameter alone.
  //
  // Default: 16
  // 
  // 健增量编码的重启点之间的健的数量。该参数可以动态更改。大多数客户端应该只保留
  // 这个参数
  int block_restart_interval;

  // Compress blocks using the specified compression algorithm.  This
  // parameter can be changed dynamically.
  //
  // Default: kSnappyCompression, which gives lightweight but fast
  // compression.
  //
  // Typical speeds of kSnappyCompression on an Intel(R) Core(TM)2 2.4GHz:
  //    ~200-500MB/s compression
  //    ~400-800MB/s decompression
  // Note that these speeds are significantly faster than most
  // persistent storage speeds, and therefore it is typically never
  // worth switching to kNoCompression.  Even if the input data is
  // incompressible, the kSnappyCompression implementation will
  // efficiently detect that and will switch to uncompressed mode.
  // 
  // 使用指定的压缩算法压缩块。该参数可以动态更改
  // 
  // 默认：kSnappyCompression，它提供了轻量级但快速的压缩
  // 
  // Intel（R）Core（TM）2 2.4GHz 上的 kSnappyCompression 的典型速度：
  //  〜200-500MB/s 压缩
  //  〜400-800MB/s 减压
  // 
  // 这些速度比大多数持久性存储速度快得多，因此通常不会切换到 kNoCompression，即使
  // 输入数据是不可压缩的，kSnappyCompression实现将有效地检测到，并将切换到未压缩
  // 模式
  CompressionType compression;

  // If non-NULL, use the specified filter policy to reduce disk reads.
  // Many applications will benefit from passing the result of
  // NewBloomFilterPolicy() here.
  //
  // Default: NULL
  // 
  // 过滤规则
  // 如果 non-NULL，则使用指定的过滤器策略来减少磁盘读取。许多应用程序将从这里传递
  // NewBloomFilterPolicy() 的结果中受益
  const FilterPolicy* filter_policy;

  // Create an Options object with default values for all fields.
  Options();
};

// Options that control read operations
// 
// 读取操作的控制选项
struct ReadOptions {
  // If true, all data read from underlying storage will be
  // verified against corresponding checksums.
  // Default: false
  // 
  // 如果为 true，则从底层存储中读取的所有数据都将要通过相应的校验和来验证
  bool verify_checksums;

  // Should the data read for this iteration be cached in memory?
  // Callers may wish to set this field to false for bulk scans.
  // Default: true
  // 
  // 应该读取这个迭代的数据缓存在内存中？对于批量扫描，调用者可能希望将此字
  // 段设置为 false
  bool fill_cache;

  // If "snapshot" is non-NULL, read as of the supplied snapshot
  // (which must belong to the DB that is being read and which must
  // not have been released).  If "snapshot" is NULL, use an implicit
  // snapshot of the state at the beginning of this read operation.
  // Default: NULL
  // 
  // 如果 "snapshot" 为 non-NULL，则从所提供的快照（必须属于正在读取的数据库
  // 以及不能释放的数据块）中读取数据。
  // 如果 "snapshot" 为 NULL，则在此读取操作的开始处使用状态的隐式快照
  const Snapshot* snapshot;

  ReadOptions()
      : verify_checksums(false),
        fill_cache(true),
        snapshot(NULL) {
  }
};

// Options that control write operations
// 
// 写入操作的控制选项
struct WriteOptions {
  // If true, the write will be flushed from the operating system
  // buffer cache (by calling WritableFile::Sync()) before the write
  // is considered complete.  If this flag is true, writes will be
  // slower.
  //
  // If this flag is false, and the machine crashes, some recent
  // writes may be lost.  Note that if it is just the process that
  // crashes (i.e., the machine does not reboot), no writes will be
  // lost even if sync==false.
  //
  // In other words, a DB write with sync==false has similar
  // crash semantics as the "write()" system call.  A DB write
  // with sync==true has similar crash semantics to a "write()"
  // system call followed by "fsync()".
  //
  // Default: false
  // 
  // 如果为 true，则在写入完成之前，写入操作将从操作系统缓冲区缓存中清除（通过
  // 调用 WritableFile::Sync() ）。如果这个标志是 true 的，写入会很慢
  // 
  // 如果这个标志是 false 的，并且机器崩溃，则最近的一些写操作可能会丢失。
  // 请注意，如果只是进程崩溃（即机器不重新启动），即使 sync==false，写入也
  // 不会丢失
  // 
  // 换句话说，使用 sync==false 的数据库写入与 "write()" 系统调用具有类似
  // 的崩溃语义。具有 sync==true 的数据库写入具有与 "write()" 系统调用，后
  // 跟 "fsync()" 有相似的崩溃语义
  bool sync;

  WriteOptions()
      : sync(false) {
  }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
