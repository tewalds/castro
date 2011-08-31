
#pragma once

#include <tr1/functional>
#include <vector>
#include <signal.h>
#include "time.h"

/* A simple alarm class that calls a callback at a certain time or after a timeout.
 * The callback is called in the interrupt context, so may be during a malloc call or similar,
 * so doing anything fancy could lead to a deadlock. It is highly suggested that the callback
 * do nothing more than change a state variable.
 *
 * Alarm() returns an Alarm object, which automatically cancels the alarm
 * when it goes out of scope, or can be cancelled directly
 * Calls all callbacks if SIGUSR1 is received
 *
 * Alarm timer(1.5, timeout_func);
 * or
 * Alarm timer;
 * timer(1.5, timeout_func);
 *
 * timer.cancel();
 */

class Alarm {
public:
	typedef std::tr1::function<void()> callback_t;

	Alarm() : id(-1) { }
	Alarm(double len, callback_t fn)   : id(-1) { (*this)(len, fn); }
	Alarm(Time timeout, callback_t fn) : id(-1) { (*this)(timeout, fn); }

	//act as a move constructor, no copy constructor
	Alarm(Alarm & o) { *this = o; }
	Alarm & operator = (Alarm & o) {
		cancel();
		id = o.id;
		o.id = -1;
		return *this;
	}

	void operator()(double len, callback_t fn){ (*this)(Time() + len, fn); }
	void operator()(Time timeout, callback_t fn){
		cancel();
		id = Handler::inst().add(timeout, fn);
	}

	~Alarm(){ cancel(); }
	bool cancel(){
		bool ret = Handler::inst().cancel(id);
		id = -1;
		return ret;
	}


private:
	int id;

	class Handler { //singleton class that handles the alarms
		struct Entry {
			int id;
			bool called;
			callback_t fn;
			Time timeout;
			Entry(int i, callback_t f, Time t) : id(i), called(false), fn(f), timeout(t) { }
		};

		int nextid;
		std::vector<Entry> alarms;

		Handler();
		Handler(Handler const& copy);            // Not Implemented
		Handler& operator=(Handler const& copy); // Not Implemented

	public:
		static Handler & inst();

		bool cancel(int id);
		void reset(int signum);
		int add(Time timeout, callback_t fn);

	};
	friend void alarm_triggered(int);
};

