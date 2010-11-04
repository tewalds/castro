
#include "havannahgtp.h"

#include <string>
using namespace std;

int main(int argc, char **argv){
	HavannahGTP gtp;

	for(int i = 1; i < argc; i++) {
		string arg = argv[i];
		if(arg == "--help"){
			printf("Usage:\n"
				"\t   --help     Show this help\n"
				"\t-v --verbose  Give more output over gtp\n"
				"\t-g --coords   Use the H-Gui coordinate system\n"
				"\t-c --cmd      Pass a gtp command from the command line\n"
				"\t-f --file     Run this gtp file before reading from stdin\n"
				"\t-l --logfile  Log the gtp commands in standard format to this file\n"
				"\t-s --server   Run in server mode\n"
				);
			exit(255);
		}else if(arg == "-v" || arg == "--verbose"){
			gtp.verbose = true;
		}else if(arg == "-g" || arg == "--coords"){
			gtp.hguicoords = true;
		}else if(arg == "-c" || arg == "--cmd"){
			char * ptr = argv[++i];
			if(ptr == NULL){ printf("Missing a command\n"); exit(255); }
			gtp.cmd(ptr);
		}else if(arg == "-f" || arg == "--file"){
			char * ptr = argv[++i];
			if(ptr == NULL){ printf("Missing a file to run\n"); exit(255); }
			FILE * fd = fopen(ptr, "r");
			gtp.setinfile(fd);
			gtp.setoutfile(NULL);
			gtp.run();
			fclose(fd);
		}else if(arg == "-l" || arg == "--logfile"){
			char * ptr = argv[++i];
			if(ptr == NULL){ printf("Missing a filename to log to\n"); exit(255); }
			FILE * fd = fopen(ptr, "w");
			if(!fd) { printf("Failed to open file %s\n", ptr); exit(255); }
			gtp.setlogfile(fd);
		}else if(arg == "-s" || arg == "--server"){
			gtp.setservermode(true);
		}else{
			printf("Unknown argument: %s\n", argv[i]);
			exit(255);
		}
	}

	gtp.setinfile(stdin);
	gtp.setoutfile(stdout);
	gtp.run();
}

