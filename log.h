
#pragma once

#include <cstdio>
#include <string>

inline void logerr(std::string str){
	fprintf(stderr, "%s", str.c_str());
}

