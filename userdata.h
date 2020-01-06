
#ifndef _USER_DATA_STORAGE__H__
#define _USER_DATA_STORAGE__H__

#include <mutex>
#include <string>
#include <stdint.h>

#include "lrucache.h"

#define SHARDF              (4)       // Sharding factor for sharded structures
#define UDATAMEM  (4*1024*1024)       // Memory allocated (aprox) for userdata

#define MAX_ENT_SHARD  ( (UDATAMEM / SHARDF) / 128 )  // Assuming each entry is around 128 bytes (with overhead)

// Class used to keep user data in memory
template <typename T>
class UserData {
public:
	UserData() {
		for (unsigned i = 0; i < SHARDF; i++)
			user_data_shards[i] = new CacheType(MAX_ENT_SHARD, MAX_ENT_SHARD/16);
	}
	~UserData() {
		for (unsigned i = 0; i < SHARDF; i++)
			delete user_data_shards[i];
	}

	bool getUserData(uint64_t userid, T *data) {
		// Acquire the read mutex for the shard
		unsigned sn = userid % SHARDF;
		if (user_data_shards[sn]->tryGet(userid, *data))
			return true;
		return false;
	}
	void updateUserData(uint64_t userid, const T &data) {
		// Acquire the write mutex for the shard
		unsigned sn = userid % SHARDF;
		user_data_shards[sn]->insert(userid, data);
	}
private:
	typedef lru11::Cache<uint64_t, T, std::mutex> CacheType;
	std::array<CacheType*, SHARDF> user_data_shards;
};

#endif

