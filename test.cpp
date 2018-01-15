#include <iostream>
#include <cassert>
#include <string>

#include <leveldb/db.h>

using namespace std;
int main(int argc, char** argv)
{
	leveldb::DB* db;
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::Status status = leveldb::DB::Open(options, "./testdb", &db);
	assert(status.ok());

	// 写入key1,value1
	std::string key = "key";
	std::string value = "value";
	status = db->Put(leveldb::WriteOptions(), key,value);
	assert(status.ok());

	// 循环写入1000000个整数.
	// key,value相同
    // for (int i = 0; i < 1000000; i++) {
    //     char str[100];
    //     snprintf(str, sizeof(str), "%d", i);
    //     status = db->Put(leveldb::WriteOptions(), str, str);
    // }

    // 获取key
	status = db->Get(leveldb::ReadOptions(), key, &value);
	assert(status.ok());
	std::cout << key << "===" << value << std::endl;

	// key,key2设置、删除、获取交错执行
	// std::string key2 = "key2";
	// status = db->Put(leveldb::WriteOptions(), key2, value);
	// assert(status.ok());

	// status = db->Delete(leveldb::WriteOptions(), key);
	// assert(status.ok());

	// status = db->Get(leveldb::ReadOptions(), key2, &value);
	// assert(status.ok());
	// std::cout << key2 << "===" << value << std::endl;

	// status = db->Get(leveldb::ReadOptions(), key, &value);
	// if (!status.ok()) {
	// 	std::cerr << key << "---" << status.ToString() << std::endl;
	// } else {
	// 	std::cout << key << "===" << value << std::endl;
	// }

	delete db;
	return 0;
}