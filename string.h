
#pragma once

//A few basic string operation helpers

#include <vector>
#include <string>
#include <sstream>

typedef std::vector<std::string> vecstr;

template <class T> std::string to_str(T a){
	std::stringstream out;
	out << a;
	return out.str();
}

std::string to_str(double a, int prec);

template <class T> T from_str(const std::string & str){
	std::istringstream sin(str);
	T ret;
	sin >> ret;
	return ret;
}


void trim(std::string & str);
void ltrim(std::string & str);
void rtrim(std::string & str);

vecstr explode(const std::string & str, const std::string & sep);
std::string implode(const vecstr & vec, const std::string & sep);

