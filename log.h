
#ifndef _LOG_H_
#define _LOG_H_

#include <cstdio>
#include <string>

inline void logerr(std::string str){
	fprintf(stderr, "%s", str.c_str());
}

#endif

