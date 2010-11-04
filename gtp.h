
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
	string id;
	string response;

	GTPResponse() { }

	GTPResponse(bool s, string r = ""){
		success = s;
		response = r;
		rtrim(response);
	}
	
	GTPResponse(string r){
		GTPResponse(true, r);
	}

	string to_s(){
		return (success ? '=' : '?') + id + ' ' + response + "\n\n";
	}
};

typedef function<GTPResponse(vecstr)> gtp_callback_fn;

struct GTPCallback {
	string name;
	string desc;
	gtp_callback_fn func;

	GTPCallback() { }
	GTPCallback(string n, string d, gtp_callback_fn fn) : name(n), desc(d), func(fn) { }
};

class GTPclient {
	FILE * in, * out, * logfile;
	bool servermode;
	vector<GTPCallback> callbacks;
	unsigned int longest_cmd;
	bool running;

public:

	GTPclient(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		in = i;
		out = o;
		logfile = l;
		servermode = false;
		longest_cmd = 0;
		running = false;

		newcallback("list_commands",    bind(&GTPclient::gtp_list_commands,    this, _1, false), "List the commands");
		newcallback("help",             bind(&GTPclient::gtp_list_commands,    this, _1, true),  "List the commands, with descriptions");
		newcallback("quit",             bind(&GTPclient::gtp_quit,             this, _1), "Quit the program");
		newcallback("exit",             bind(&GTPclient::gtp_quit,             this, _1), "Alias for quit");
		newcallback("protocol_version", bind(&GTPclient::gtp_protocol_version, this, _1), "Show the gtp protocol version");
		newcallback("logfile",          bind(&GTPclient::gtp_logfile,          this, _1), "Start logging the gtp commands to this file, disabled in server mode");
		newcallback("lognote",          bind(&GTPclient::gtp_lognote,          this, _1), "Add a comment to the logfile");
		newcallback("logend",           bind(&GTPclient::gtp_logend,           this, _1), "Stop logging");
	}

	~GTPclient(){
		if(logfile)
			fclose(logfile);
	}

	void setservermode(bool m = true){
		servermode = m;
	}

	void setinfile(FILE * i){
		in = i;
	}

	void setoutfile(FILE * o){
		out = o;
	}

	void setlogfile(FILE * l){
		if(logfile)
			fclose(logfile);
		logfile = l;
	}

	void newcallback(const string name, const gtp_callback_fn & fn, const string desc = ""){
		newcallback(GTPCallback(name, desc, fn));
		if(longest_cmd < name.length())
			longest_cmd = name.length();
	}
	
	void newcallback(const GTPCallback & a){
		callbacks.push_back(a);
	}
	
	int find_callback(const string & name){
		for(unsigned int i = 0; i < callbacks.size(); i++)
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

	GTPResponse cmd(string line){
		vecstr parts = explode(line, " ");
		string id;

		if(parts.size() > 1 && atoi(parts[0].c_str())){
			id = parts[0];
			parts.erase(parts.begin());
		}

		string name = parts[0];
		parts.erase(parts.begin());

		int cb = find_callback(name);
		GTPResponse response;

		if(cb < 0){
			response = GTPResponse(false, "Unknown command");
		}else{
			response = callbacks[cb].func(parts);
		}

		response.id = id;

		return response;
	}

	void run(){
		running = true;
		char buf[1001];

		while(running && fgets(buf, 1000, in)){
			string line(buf);

			trim(line);
			
			if(line.length() == 0 || line[0] == '#')
				continue;

			GTPResponse response = cmd(line);

			if(out){
				string output = response.to_s();

				fwrite(output.c_str(), 1, output.length(), out);
				fflush(out);
			}
		}
		running = false;
	}

	GTPResponse gtp_logfile(vecstr args){
		if(servermode)
			return GTPResponse(false, "Logs disabled");

		if(args.size() != 1)
			return GTPResponse(false, "Wrong number of arguments");

		FILE * fd = fopen(args[0].c_str(), "w");
		if(!fd)
			return GTPResponse(false, "Couldn't open file " + args[0]);

		setlogfile(fd);

		return GTPResponse(true);
	}

	GTPResponse gtp_lognote(vecstr args){
		if(servermode)
			return GTPResponse(false, "Logs disabled");

		log(implode(args, " "));
		return GTPResponse(true);
	}

	GTPResponse gtp_logend(vecstr args){
		if(servermode)
			return GTPResponse(false, "Logs disabled");

		setlogfile(NULL);
		return GTPResponse(true);
	}

	GTPResponse gtp_protocol_version(vecstr args){
		return GTPResponse(true, "2");
	}

	GTPResponse gtp_quit(vecstr args){
		running = false;
		return true;
	}
	
	GTPResponse gtp_list_commands(vecstr args, bool showdesc){
		string ret = "\n";
		for(unsigned int i = 0; i < callbacks.size(); i++){
			ret += callbacks[i].name;
			if(showdesc && callbacks[i].desc.length() > 0){
				ret += string(longest_cmd + 2 - callbacks[i].name.length(), ' ');
				ret += callbacks[i].desc;
			}
			ret += "\n";
		}
		return GTPResponse(true, ret);
	}
};

#endif

