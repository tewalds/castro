
#pragma once

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

#ifdef SINGLE_THREAD

//#define CAS(var, old, new) (((var) == (old)) ? ((var) = (new), true) : false )
//#define INCR(var)          (++(var))
//#define PLUS(var, val)     ((var) += (val))

template<class A, class B, class C> bool CAS(A & var, const B & old, const C & newval){
	if(var == old){
		var = newval;
		return true;
	}
	return false;
}

template<class A> A INCR(A & var){
	return ++var;
}

template<class A, class B> A PLUS(A & var, const B & val){
	return (var += val);
}

#else

//#define CAS(var, old, new) __sync_bool_compare_and_swap(&(var), old, new)
//#define INCR(var)          __sync_add_and_fetch(&(var), 1)
//#define PLUS(var, val)     __sync_add_and_fetch(&(var), val)

template<class A, class B, class C> bool CAS(A & var, const B & old, const C & newval){
	return __sync_bool_compare_and_swap(& var, old, newval);
}

template<class A> A INCR(A & var){
	return __sync_add_and_fetch(& var, 1);
}

template<class A, class B> A PLUS(A & var, const B & val){
	return __sync_add_and_fetch(& var, val);
}

#endif

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
		o.func = nullfunc;

		return *this;
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

class SpinLock {
	int taken;
public:
	SpinLock() : taken(0) { }
	void lock()   { while(!trylock()) ; }
#ifdef SINGLE_THREAD
	bool trylock(){ return true; }
	void unlock() { };
#else
	bool trylock(){ return !__sync_lock_test_and_set(&taken, 1); }
	void unlock() { __sync_lock_release(&taken); }
#endif
};

//object wrapper around pthread rwlock
class RWLock {
	pthread_rwlock_t rwlock;

public:
	RWLock(bool preferwriter = true){
		pthread_rwlockattr_t attr;
		pthread_rwlockattr_init(&attr);

#ifndef __APPLE__
		if(preferwriter)
			pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

		pthread_rwlock_init(&rwlock, &attr);

		pthread_rwlockattr_destroy(&attr);
	}
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
		t.tv_nsec = (long int)((timeout - (int)timeout) * 1000000000);
		return pthread_cond_timedwait(&cond, &mutex, &t);
	}
};

//*
class Barrier {
	static const uint32_t waitexit = (1<<30);
	CondVar cond;
	volatile uint32_t numthreads, //number of threads to wait for
	                  counter;    //number of threads currently waiting

//not copyable
	Barrier(const Barrier & b) { }
	Barrier operator=(const Barrier & b);

public:
	Barrier(int n = 1) : numthreads(n), counter(0) { }
	void reset(int n){
		cond.lock();
		assert(counter == 0);
		numthreads = n;
		cond.unlock();
	}

	~Barrier(){
		cond.lock();

		//wait for threads to exit
		while(counter > waitexit)
			cond.wait();

		cond.unlock();
	}

	bool wait(){
		cond.lock();

		//wait for threads to exit
		while(counter > waitexit)
			cond.wait();

		bool flip = (++counter == numthreads);

		if(flip){
			counter--;
			if(numthreads > 1)
				counter += waitexit;
			cond.broadcast();
		}else{
			while(counter < waitexit)
				cond.wait();

			counter--;
			if(counter == waitexit){
				counter = 0;
				cond.broadcast();
			}
		}
		cond.unlock();

		return flip;
	}
};
/*/
//use the pthread Barrier implementation
//the destructor below seems to have a deadlock that I can't explain but that doesn't affect the custom version above
class Barrier {
	pthread_barrier_t barrier;

//not copyable
	Barrier(const Barrier & b) { }
	Barrier operator=(const Barrier & b);

public:
	Barrier(int n = 1) { pthread_barrier_init(&barrier, NULL, n); }
	~Barrier()         { pthread_barrier_destroy(&barrier); }

	void reset(int n){
		pthread_barrier_destroy(&barrier);
		pthread_barrier_init(&barrier, NULL, n);
	}

	bool wait(){
		return !(pthread_barrier_wait(&barrier) == 0);
	}
};
//*/

