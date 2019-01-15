
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
	struct t_query {
		std::string query;
		uint8_t offset;
	};

	bool getLastQuery(uint64_t userid, t_query *query) {
		// Acquire the read mutex for the shard
		unsigned sn = userid % SHARDF;
		if (user_data_shards[sn]->tryGet(userid, *query))
			return true;
		return false;
	}
	void updateLastQuery(uint64_t userid, const t_query &query) {
		// Acquire the write mutex for the shard
		unsigned sn = userid % SHARDF;
		user_data_shards[sn]->insert(userid, query);
	}
private:
	typedef lru11::Cache<uint64_t, t_query, std::mutex> CacheType;
	std::array<CacheType*, SHARDF> user_data_shards;
};

#endif

