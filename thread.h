


#ifndef _MY_THREAD_H_
#define _MY_THREAD_H_

#include <tr1/functional>
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <time.h>

using namespace std;
using namespace tr1;
using namespace placeholders; //for bind

// http://gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2427.html
#define CAS(var, old, new) __sync_bool_compare_and_swap(&(var), old, new)
#define INCR(var)          __sync_add_and_fetch(&(var), 1)
#define PLUS(var, val)     __sync_add_and_fetch(&(var), val)


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

	//act as a move constructor, no copy constructor
	Thread(Thread & o) { *this = o; }
	Thread & operator = (Thread & o) {
		assert(destruct == false); //don't overwrite a thread

		thread = o.thread;
		destruct = o.destruct;
		func = o.func;

		//set the other object to no longer own the thread
		o.thread = pthread_t();
		o.destruct = false;
		func = nullfunc;
	}

	int operator()(function<void()> fn){
		assert(destruct == false);

		func = fn;
		destruct = true;
		return pthread_create(&thread, NULL, (void* (*)(void*)) &Thread::runner, this);
	}

	int detach(){ assert(destruct == true); return pthread_detach(thread); }
	int join()  { assert(destruct == true); destruct = false; return pthread_join(thread, NULL); }
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
protected:
	pthread_mutex_t mutex;

public:
	Mutex()  { pthread_mutex_init(&mutex, NULL); }
	~Mutex() { pthread_mutex_destroy(&mutex); }
	
	int lock()    { return pthread_mutex_lock(&mutex); }
	int trylock() { return pthread_mutex_trylock(&mutex); }
	int unlock()  { return pthread_mutex_unlock(&mutex); }
};

class CondVar : public Mutex {
protected:
	pthread_cond_t cond;

public:
	CondVar() { Mutex(); pthread_cond_init(&cond, NULL);}
	~CondVar(){ pthread_cond_destroy(&cond); }

	int broadcast(){ return pthread_cond_broadcast(&cond); }
	int signal()   { return pthread_cond_signal(&cond); }

	int wait(){ return pthread_cond_wait(&cond, &mutex); }
	int timedwait(double timeout){
		timespec t;
		t.tv_sec = (int)timeout;
		t.tv_nsec = (timeout - (int)timeout) * 1000000000;
		return pthread_cond_timedwait(&cond, &mutex, &t);
	}
};

#endif

