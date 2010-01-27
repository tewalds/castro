
#ifndef _GTP_H_
#define _GTP_H_

#include "string.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <tr1/functional>

using namespace std;
using namespace tr1;
using namespace placeholders; //for bind

struct GTPResponse {
	bool success;
	string response;

	GTPResponse() { }

	GTPResponse(bool s, string r = ""){
		success = s;
		response = r;
	}
	
	GTPResponse(string r){
		success = true;
		response = r;
	}
};

typedef function<GTPResponse(vecstr)> gtp_callback_fn;

struct GTPCallback {
	string name;
	gtp_callback_fn func;

	GTPCallback() { }
	GTPCallback(string n, gtp_callback_fn fn){
		name = n;
		func = fn;
	}
};

class GTPclient {
	FILE * in, * out, * logfile;
	vector<GTPCallback> callbacks;

public:

	GTPclient(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		in = i;
		out = o;
		logfile = l;

		newcallback("help",             bind(&GTPclient::gtp_list_commands,    this, _1));
		newcallback("list_commands",    bind(&GTPclient::gtp_list_commands,    this, _1));
		newcallback("quit",             bind(&GTPclient::gtp_quit,             this, _1));
		newcallback("protocol_version", bind(&GTPclient::gtp_protocol_version, this, _1));
		newcallback("logfile",          bind(&GTPclient::gtp_logfile,          this, _1));
		newcallback("logend",           bind(&GTPclient::gtp_logend,           this, _1));
	}

	~GTPclient(){
		if(logfile)
			fclose(logfile);
	}

	void setlogfile(FILE * l){
		if(logfile)
			fclose(logfile);
		logfile = l;
	}

	void newcallback(const string name, const gtp_callback_fn & fn){
		newcallback(GTPCallback(name, fn));
	}
	
	void newcallback(const GTPCallback & a){
		callbacks.push_back(a);
	}
	
	int find_callback(const string & name){
		for(int i = 0; i < callbacks.size(); i++)
			if(callbacks[i].name == name)
				return i;
		return -1;
	}

	void log(const string & str){
		if(logfile){
			fprintf(logfile, "%s\n", str.c_str());
			fflush(logfile);
		}
	}

	void run(){
		char buf[1001];

		while(fgets(buf, 1000, in)){
			string line(buf);

			trim(line);
			
			if(line.length() == 0 || line[0] == '#')
				continue;

			vecstr parts = explode(line, " ");
			string id;

			if(atoi(parts[0].c_str())){
				id = parts[0];
				parts.erase(parts.begin());
			}

			if(parts.size() == 0)
				continue;

			string name = parts[0];
			parts.erase(parts.begin());

			int cb = find_callback(name);
			GTPResponse response;
		
			
			if(cb < 0){
				response = GTPResponse(false, "Unknown command");
			}else{
				response = callbacks[cb].func(parts);
			}

			string output = (response.success ? '=' : '?') + id + ' ' + response.response + "\n\n";

			fwrite(output.c_str(), 1, output.length(), out);
			fflush(out);

			if(name == "quit")
				break;
		}
	}

	GTPResponse gtp_logfile(vecstr args){
		if(args.size() != 1)
			return GTPResponse(false, "Wrong number of arguments");

		FILE * fd = fopen(args[0].c_str(), "w");
		if(!fd)
			return GTPResponse(false, "Couldn't open file " + args[0]);

		setlogfile(fd);

		return GTPResponse(true);
	}

	GTPResponse gtp_logend(vecstr args){
		if(logfile)
			fclose(logfile);
		return GTPResponse(true);
	}

	GTPResponse gtp_protocol_version(vecstr args){
		return GTPResponse(true, "2");
	}

	GTPResponse gtp_quit(vecstr args){
		return GTPResponse(true);
	}
	
	GTPResponse gtp_list_commands(vecstr args){
		string ret;
		for(int i = 0; i < callbacks.size(); i++)
			ret += callbacks[i].name + "\n";
		return GTPResponse(true, ret);
	}
};

#endif

