
#include <string>
#include <iostream>

using namespace std;


#include "proofdb.h"


int main(int argc, char** argv) {

	if(argc == 1){
		cout << "Usage: " << argv[0] << " dbname <inputs ...>\n";
		return 0;
	}

	// create the database object
	ProofDB<MCTSRec> db(argv[1]);

	for(int i = 2; i < argc; i++) {
		string arg = argv[i];

		string line;
		ifstream file(arg.c_str());
		while(getline(file, line)){
//			cout << line << endl;
			MCTSRec in(line);
			MCTSRec old = in;
			if(db.get(old)){
				if(old.outcome != in.outcome) //hash collision, set it to be resolved
					in.outcome = -3;
				else if(in.work > old.work)
					continue;
			}
			db.set(in);
		}
		file.close();
	}

	db.close();

	return 0;
}

