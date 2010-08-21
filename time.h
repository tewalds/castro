
#ifndef _TIME_H_
#define _TIME_H_

#include <time.h>
#include <sys/time.h>

inline int time_msec(){
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec*1000 + time.tv_usec/1000);
}

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

	int to_i()    const { return (int)t; }
	int in_msec() const { return (int)(t*1000); }
	int in_usec() const { return (int)(t*1000000); }
	double to_f() const { return t; }

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

