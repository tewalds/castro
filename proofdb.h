
#pragma once

#include <kcpolydb.h>
#include "string.h"

struct MCTSRec {
	uint64_t hash;
	uint64_t work;
	int16_t  outcome;

	MCTSRec(uint64_t h = 0) : hash(h), work(0), outcome(-4) { }

	MCTSRec(const std::string & line){
		vecstr parts = explode(line, ",");
		hash         = stru64(parts[0]);
		work         = from_str<uint64_t>(parts[1]);
		outcome      = from_str<uint16_t>(parts[2]);
	}

	MCTSRec(const std::string & key, const std::string & val){
		hash = stru64(key);
		set_value(val);
	}

	void set_value(const std::string & val){
		vecstr parts = explode(val, ",");
		work         = from_str<uint64_t>(parts[0]);
		outcome      = from_str<uint16_t>(parts[1]);
	}

	std::string key() const {
		char buf[17];
		u64buf(buf, hash);
		return std::string("0x") + buf;
	}
	std::string value() const {
		return to_str(work) + "," + to_str(outcome);
	}

	static uint64_t stru64(const std::string & s) { return stru64(s.c_str()); }
	static uint64_t stru64(const char * c) {
		if(c[0] == '0' && c[1] == 'x')
			c += 2;
		uint64_t val = 0;
		for(int i = 0; i < 16; i++, c++)
			val = (val << 4) | (uint64_t)(*c <= '9' ? *c - '0' : *c - 'a' + 10);
		return val;
	}

	static void u64buf(char * buf, uint64_t val) {
		static const char hexlookup[] = "0123456789abcdef";
		for(int i = 15; i >= 0; i--){
			buf[i] = hexlookup[val & 15];
			val >>= 4;
		}
		buf[16] = '\0';
	}
};

struct PNSRec {
	string   boardstr;
	uint64_t work;
	int16_t  outcome;

	PNSRec(string b = "") : boardstr(b), work(0), outcome(-4) { }

	PNSRec(const std::string & key, const std::string & val){
		boardstr = key;
		set_value(val);
	}

	void set_value(const std::string & val){
		vecstr parts = explode(val, ",");
		work         = from_str<uint64_t>(parts[0]);
		outcome      = from_str<uint16_t>(parts[1]);
	}

	std::string key() const {
		return boardstr;
	}
	std::string value() const {
		return to_str(work) + "," + to_str(outcome);
	}
};


template<class ProofRec> class ProofDB {
//	kyotocabinet::PolyDB db;
//	kyotocabinet::HashDB db;
	kyotocabinet::TreeDB db;
public:
	ProofDB() { }
	ProofDB(const std::string & name) { open(name); }

	bool open(const std::string & name){
		db.tune_map(4LL << 30); //4gb
		db.tune_page_cache(2LL << 30); //2gb
//		db.tune_buckets(100LL * 1000*1000); //100 million buckets
		return db.open(name, kyotocabinet::PolyDB::OWRITER | kyotocabinet::PolyDB::OCREATE);
	}
	bool close()     { return db.close(); }
	bool clear()     { return db.clear(); }
	int64_t count()  { return db.count(); }
	int64_t size()   { return db.size();  }
	bool get(ProofRec & rec) {
		std::string value;
		bool ret = db.get(rec.key(), &value);
		if(ret)
			rec.set_value(value);
		return ret;
	}
	bool set(const ProofRec & rec) {
		return db.set(rec.key(), rec.value());
	}
};

