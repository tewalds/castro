


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

//object wrapper around pthread rwlock
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

//similar to RWLock, but gives priority to writers
class RWLock2 {
	RWLock  lock; // stop the thread runners from doing work
	volatile int stop; // write lock asked
	volatile bool writelock; //is the write lock

public:
	RWLock2() : stop(0), writelock(false) { }

	bool is_wrlocked(){ return writelock; }
	int wrlock(){ //aquire the write lock, set stop, blocks
//		fprintf(stderr, "ask write lock\n");

		stop = true;
		int r = lock.wrlock();
		writelock = true;
		stop = false;

//		fprintf(stderr, "got write lock\n");

		return r;
	}
	int rdlock(){ //aquire the read lock, blocks
//		fprintf(stderr, "Ask for read lock\n");
		if(stop){ //spin on the read lock so that the write lock isn't starved
//			fprintf(stderr, "Spinning on read lock\n");
			while(stop)
				sleep(0);
//			fprintf(stderr, "Done spinning on read lock\n");
		}
		int ret = lock.rdlock();
//		fprintf(stderr, "Got a read lock\n");
		return ret;
	}
	int tryrdlock(){
		return lock.tryrdlock();
	}
	int relock(){ //succeeds if stop isn't requested
		return (stop == false);
	}
	int unlock(){ //unlocks the lock
//		if(writelock) fprintf(stderr, "unlock write lock\n");
//		else          fprintf(stderr, "unlock read lock\n");

		writelock = false;
		return lock.unlock();
	}
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

class Barrier {
private:
	CondVar cond;
	volatile int count,   //number of threads to wait for
	             waiting; //number of threads waiting

//not copyable
	Barrier(const Barrier & b) { }
	Barrier operator=(const Barrier & b) { return Barrier(count); }

public:
	Barrier(int n) : count(n), waiting(0) { }

	bool wait(){
		cond.lock();
		bool flip = (++waiting == count);

		if(flip){
			waiting = 0;
			cond.broadcast();
		}else{
			while(waiting < count)
				cond.wait();
		}
		cond.unlock();

		return flip;
	}
};


#endif

