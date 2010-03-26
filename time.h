
#ifndef _TIME_H_
#define _TIME_H_

#include <time.h>
#include <sys/time.h>

int time_msec();

class Time {
	double t;

	Time(){
		struct timeval time;
		gettimeofday(&time, NULL);
		t = time.tv_sec + (double)time.tv_usec/1000000;
	}
	Time(double a) : t(a) { }

	int to_i()   { return t; }
	int in_msec(){ return t*1000; }
	int in_usec(){ return t*1000000; }
	double to_f(){ return t; }

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

#endif

