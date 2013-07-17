
#include "alarm.h"
#include "timer.h"

void alarm_triggered(int signum){
	Alarm::Handler::inst().reset(signum);
}

void timer_triggered(){
	alarm_triggered(SIGALRM);
}

Timer & timer() {
	static Timer t;
	return t;
}

Alarm::Handler::Handler() : nextid(0) {
	timer();
}

Alarm::Handler & Alarm::Handler::inst() {
	static Handler instance;
	return instance;
}

int Alarm::Handler::add(Time timeout, callback_t fn){
	Entry entry(nextid, fn, timeout);
	alarms.push_back(entry);

	nextid = (nextid + 1) & 0x3FFFFFFF; //sets a limit of 2^30 alarms, but keeps them in the positive range

	reset(SIGALRM);

	return entry.id;
}

bool Alarm::Handler::cancel(int id){
	if(id < 0)
		return false;
	bool found = false;
	for(std::vector<Entry>::iterator a = alarms.begin(); a != alarms.end(); ){
		if(a->id == id){
			a->called = true;
			found = true;
		}
		if(a->called)
			a = alarms.erase(a);
		else
			++a;
	}
	return found;
}

void Alarm::Handler::reset(int signum){
	Time now;

	double next = 0;
	for(std::vector<Entry>::iterator a = alarms.begin(); a != alarms.end(); ++a){
		double cur = a->timeout - now;
		if(cur <= 0 || signum == SIGUSR1){
			if(!a->called){
				a->fn();
				a->called = true;
			}
		}else{
			if(next == 0 || next > cur)
				next = cur;
		}
	}

	if(next > 0)
		timer().set(next, timer_triggered);
}
