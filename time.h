
#pragma once

#include <time.h>
#include <sys/time.h>

class Time {
	double t;
public:
	Time(){
		struct timeval time;
		gettimeofday(&time, NULL);
		t = time.tv_sec + (double)time.tv_usec/1000000;
	}
	Time(double a) : t(a) { }
	Time(const struct timeval & time){
		t = time.tv_sec + (double)time.tv_usec/1000000;
	}

	double    to_f()    const { return t; }
	long long to_i()    const { return (long long)t; }
	long long in_msec() const { return (long long)(t*1000); }
	long long in_usec() const { return (long long)(t*1000000); }

	Time   operator +  (double a)       const { return Time(t+a); }
	Time & operator += (double a)             { t += a; return *this; }
	double operator -  (const Time & a) const { return t - a.t; }
	Time   operator -  (double a)       const { return Time(t-a); }
	Time & operator -= (double a)             { t -= a; return *this; }

	bool operator <  (const Time & a) const { return t <  a.t; }
	bool operator <= (const Time & a) const { return t <= a.t; }
	bool operator >  (const Time & a) const { return t >  a.t; }
	bool operator >= (const Time & a) const { return t >= a.t; }
	bool operator == (const Time & a) const { return t == a.t; }
	bool operator != (const Time & a) const { return t != a.t; }
};

