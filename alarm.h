
#pragma once

#include <tr1/functional>
#include <vector>
#include <signal.h>
#include "time.h"

/* A simple alarm class that calls a callback at a certain time or after a timeout.
 * Alarm::set() returns a Alarm::Ctrl object, which automatically cancels the alarm
 * when it goes out of scope, or can be cancelled directly
 *
 * Alarm::Ctrl timer(1.5, timeout_func);
 * timer.cancel();
 */

class Alarm {
public:
	typedef std::tr1::function<void()> callback_t;
	class Ctrl {
		int id;
		Ctrl(int i = -1) : id(i) { }
	public:
		Ctrl() : id(-1) { }
//		Ctrl(Ctrl & other) { id = other.id; other.id = -1; }
//		Ctrl & operator=(Ctrl & other){ if(this != &other){ cancel(); id = other.id; other.id = -1; } return *this; }
//		Ctrl & move(Ctrl & other){ if(this != &other){ cancel(); id = other.id; other.id = -1; } return *this; }

		Ctrl(Ctrl & o) { *this = o; }
		Ctrl & operator = (Ctrl & o) {
			cancel();
			id = o.id;
			o.id = -1;
			return *this;
		}

		~Ctrl(){ cancel(); }
		bool cancel();
	};

private:
	struct Entry {
		int id;
		callback_t fn;
		Time timeout;
		Entry(int i, callback_t f, Time t) : id(i), fn(f), timeout(t) { }
	};

	int nextid;
	std::vector<Entry> alarms;

	Alarm();
	Alarm(Alarm const& copy);            // Not Implemented
	Alarm& operator=(Alarm const& copy); // Not Implemented

	static Alarm & inst();

	bool cancel(int id);
	void reset();
	Ctrl add(Time timeout, callback_t fn);

	friend void alarm_triggered(int);

public:

	static Ctrl set(double len, callback_t fn)   { return Alarm::inst().add(Time() + len, fn); }
	static Ctrl set(Time timeout, callback_t fn) { return Alarm::inst().add(timeout, fn); }
};

