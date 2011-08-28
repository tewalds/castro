
#include "alarm.h"

void alarm_triggered(int blah){
	Alarm::inst().reset();
}

Alarm::Alarm() : nextid(0) {
	signal(SIGALRM, alarm_triggered);
}

Alarm & Alarm::inst() {
	static Alarm instance;
	return instance;
}

Alarm::Ctrl Alarm::add(Time timeout, callback_t fn){
	Entry entry(nextid, fn, timeout);
	alarms.push_back(entry);

	nextid = (nextid + 1) & 0x3FFFFFFF; //sets a limit of 2^30 alarms, but keeps them in the positive range

	reset();

	return Ctrl(entry.id);
}

bool Alarm::Ctrl::cancel(){
	bool ret = Alarm::inst().cancel(id);
	id = -1;
	return ret;
}

bool Alarm::cancel(int id){
	if(id < 0)
		return false;
	for(std::vector<Entry>::iterator a = alarms.begin(); a != alarms.end(); ++a){
		if(a->id == id){
			alarms.erase(a);
			return true;
		}
	}
	return false;
}

void Alarm::reset(){
	Time now;

	double next = 0;
	for(std::vector<Entry>::iterator a = alarms.begin(); a != alarms.end(); ){
		double cur = a->timeout - now;
		if(cur <= 0){
			a->fn();
			a = alarms.erase(a);
		}else{
			if(next == 0 || next > cur)
				next = cur;
			++a;
		}
	}

	if(next > 0){
		struct itimerval timeout;
		timeout.it_interval.tv_usec = 0;
		timeout.it_interval.tv_sec = 0;
		timeout.it_value.tv_usec = (next - (int)next)*1000000;
		timeout.it_value.tv_sec = next;
		setitimer(ITIMER_REAL, &timeout, NULL);
	}
}

