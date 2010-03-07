
#ifndef _TIMER_H_
#define _TIMER_H_

#include <tr1/functional>
#include <pthread.h>
#include <unistd.h>

using namespace std;
using namespace tr1;
using namespace placeholders; //for bind

class Timer {
	pthread_t thread;
	bool destruct;
	function<void()> callback;
	double timeout;

	static void * waiter(void * blah){
		Timer * timer = (Timer *)blah;
		sleep((int)timer->timeout);
		usleep((int)((timer->timeout - (int)timer->timeout)*1000000));
		timer->callback();
		return NULL;
	}

	static void nullcallback(){ }

public:
	Timer(double time, function<void()> fn){
		timeout = time;
		callback = fn;
		if(time < 0.001){ //too small, just call it directly
			destruct = false;
			fn();
		}else{
			destruct = true;
			pthread_create(&thread, NULL, (void* (*)(void*)) &Timer::waiter, this);
		}
	}

	void cancel(){
		if(destruct){
			destruct = false;
			callback = &Timer::nullcallback;
			pthread_cancel(thread);
			pthread_join(thread, NULL);
		}
	}

	~Timer(){
		cancel();
	}
};

#endif

