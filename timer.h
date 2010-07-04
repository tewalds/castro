
#ifndef _TIMER_H_
#define _TIMER_H_

#include <tr1/functional>
#include <unistd.h>
#include "thread.h"

using namespace std;
using namespace tr1;
using namespace placeholders; //for bind

class Timer {
	Thread thread;
	bool destruct;
	function<void()> callback;
	double timeout;

	void waiter(){
		sleep((int)timeout);
		usleep((int)((timeout - (int)timeout)*1000000));
		callback();
	}

	void nullcallback(){ }

public:
	Timer() {
		timeout = 0;
		destruct = false;
		callback = bind(&Timer::nullcallback, this);
	}

	Timer(double time, function<void()> fn){
		destruct = false;
		set(time, fn);
	}

	void set(double time, function<void()> fn){
		cancel();

		timeout = time;
		callback = fn;
		if(time < 0.001){ //too small, just call it directly
			destruct = false;
			fn();
		}else{
			destruct = true;
			thread(bind(&Timer::waiter, this));
		}
	}

	void cancel(){
		if(destruct){
			destruct = false;
			callback = bind(&Timer::nullcallback, this);
			thread.cancel();
			thread.join();
		}
	}

	~Timer(){
		cancel();
	}
};

#endif

