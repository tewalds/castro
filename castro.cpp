
#include "havannahgtp.h"

#include <unistd.h>
#include <string>
using namespace std;

void die(int code, const string & str){
	printf("%s\n", str.c_str());
	exit(code);
}

int main(int argc, char **argv){
	HavannahGTP gtp;

	gtp.colorboard = isatty(fileno(stdout));

	for(int i = 1; i < argc; i++) {
		string arg = argv[i];
		if(arg == "-h" || arg == "--help"){
			die(255, "Usage:\n"
				"\t-h --help     Show this help\n"
				"\t-v --verbose  Give more output over gtp\n"
				"\t-n --nocolor  Don't output the board in color\n"
				"\t-c --cmd      Pass a gtp command from the command line\n"
				"\t-f --file     Run this gtp file before reading from stdin\n"
				"\t-l --logfile  Log the gtp commands in standard format to this file\n"
//				"\t-s --server   Run in server mode\n"
				);
		}else if(arg == "-v" || arg == "--verbose"){
			gtp.verbose = true;
		}else if(arg == "-n" || arg == "--nocolor"){
			gtp.colorboard = false;
		}else if(arg == "-c" || arg == "--cmd"){
			char * ptr = argv[++i];
			if(ptr == NULL) die(255, "Missing a command");
			gtp.cmd(ptr);
		}else if(arg == "-f" || arg == "--file"){
			char * ptr = argv[++i];
			if(ptr == NULL) die(255, "Missing a file to run");
			FILE * fd = fopen(ptr, "r");
			gtp.setinfile(fd);
			gtp.setoutfile(NULL);
			if(!gtp.run())
				return 0;
			fclose(fd);
		}else if(arg == "-l" || arg == "--logfile"){
			char * ptr = argv[++i];
			if(ptr == NULL) die(255, "Missing a filename to log to");
			FILE * fd = fopen(ptr, "w");
			if(!fd) die(255, string("Failed to open file: ") + ptr);
			gtp.setlogfile(fd);
//		}else if(arg == "-s" || arg == "--server"){
//			gtp.setservermode(true);
		}else{
			die(255, "Unknown argument: " + arg + ", try --help");
		}
	}

	gtp.setinfile(stdin);
	gtp.setoutfile(stdout);
	gtp.run();
	return 0;
}

