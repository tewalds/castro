


#ifndef _MY_THREAD_H_
#define _MY_THREAD_H_

#include <tr1/functional>
#include <pthread.h>
#include <unistd.h>
#include <cassert>

using namespace std;
using namespace tr1;
using namespace placeholders; //for bind

class Thread {
	pthread_t thread;
	bool destruct;
	function<void()> func;

	static void * runner(void * blah){
		Thread * t = (Thread *)blah;
		t->func();
		return NULL;
	}

	static void nullfunc(){ assert(false && "How did Thread::nullfunc get called?"); }

public:
	Thread()                    : destruct(false), func(nullfunc) { }
	Thread(function<void()> fn) : destruct(false), func(nullfunc) { (*this)(fn); }

	Thread(const Thread & o) { assert(false && "No copy constructor for Thread!\n"); }
	Thread & operator = (const Thread & o) { assert(false && "No copy operator for Thread!\n"); }

	int operator()(function<void()> fn){
		assert(destruct == false);

		func = fn;
		destruct = true;
		return pthread_create(&thread, NULL, (void* (*)(void*)) &Thread::runner, this);
	}

	int detach(){ assert(destruct == true); return pthread_detach(thread); }
	int join()  { assert(destruct == true); return pthread_join(thread, NULL); destruct = false; }
	int cancel(){ assert(destruct == true); return pthread_cancel(thread); }

	~Thread(){
		if(destruct){
			cancel();
			join();
		}
	}
};


class RWLock {
	pthread_rwlock_t rwlock;

public:
	RWLock(){ pthread_rwlock_init(&rwlock, NULL); }
	~RWLock(){ pthread_rwlock_destroy(&rwlock); }

	int rdlock()    { return pthread_rwlock_rdlock(&rwlock); }
	int tryrdlock() { return pthread_rwlock_tryrdlock(&rwlock); }

	int wrlock()    { return pthread_rwlock_wrlock(&rwlock); }
	int trywrlock() { return pthread_rwlock_trywrlock(&rwlock); }

	int unlock()    { return pthread_rwlock_unlock(&rwlock); }
};

class Mutex {
	pthread_mutex_t mutex;

public:
	Mutex()  { pthread_mutex_init(&mutex, NULL); }
	~Mutex() { pthread_mutex_destroy(&mutex); }
	
	int lock()    { return pthread_mutex_lock(&mutex); }
	int trylock() { return pthread_mutex_trylock(&mutex); }
	int unlock()  { return pthread_mutex_unlock(&mutex); }
};

#endif

