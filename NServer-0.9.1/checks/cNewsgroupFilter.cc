#include <iostream>

#include "NewsgroupFilter.h"

#define V(x) if(verbose) { x; }

NewsgroupFilter filters[] = {
	NewsgroupFilter("at.*"),
	NewsgroupFilter("comp.*"),
	NewsgroupFilter("comp.*,!comp.os.windows.*"),
	NewsgroupFilter("at.*,!at.secret.*,!at.top.secret.*"),
	NewsgroupFilter("at.*,!at.top.secret.*,comp.os.linux.*"),
	NewsgroupFilter("at.*,!at.top.*"),
	NewsgroupFilter("at.*,!at.top.top.top.*"),
	NewsgroupFilter("at.top.top.*,!at.top.top.top.*")
};
int filters_n = sizeof(filters) / sizeof(filters[0]);

const char *newsgroups[] = {
	"at.tuwien.general",
	"comp.os.linux",
	"at.top.secret.aaa",
	"at.top.top.secret.xxx",
	"at.top.top.top.secret.yyy",
	"at.top.top.top.top.secret.zzz",
	"at.secret.bbb",
	"foo.bar"
};
int newsgroups_n = sizeof(newsgroups) / sizeof(newsgroups[0]);

int ef = 0;
int verbose = 0;

#define MAX(X,Y) (((X)>(Y))?(X):(Y))
#define MIN(X,Y) (((X)<(Y))?(X):(Y))

class CheckAND {
      public:
	void operator() (const NewsgroupFilter & f1,
			 const NewsgroupFilter & f2) {
		NewsgroupFilter f1OPf2(f1);
		 f1OPf2 &= f2;

		for (int i = 0; i < newsgroups_n; i++) {
			int f1OPf2m =
			    (f1OPf2.matches(newsgroups[i]) > 0) ? 1 : -1;
			int f1OPf2n = (f1.matches(newsgroups[i]) > 0
				       && f2.matches(newsgroups[i]) >
				       0) ? 1 : -1;

			if (f1OPf2n == f1OPf2m) {
				cout << "+" << flush;
			} else {
				cout << "-" << flush;
				ef++;
				V(cout << endl;
				  cout << "{" << f1 << "} and {" << f2 <<
				  "}=={" << f1OPf2 << "}" << endl;
				  cout << newsgroups[i] << flush;
				    );
			}
		}
		cout << " " << flush;
	}
};

class CheckOR {
      public:
	void operator() (const NewsgroupFilter & f1,
			 const NewsgroupFilter & f2) {
		NewsgroupFilter f1OPf2(f1);
		 f1OPf2 |= f2;

		for (int i = 0; i < newsgroups_n; i++) {
			int f1OPf2m =
			    (f1OPf2.matches(newsgroups[i]) > 0) ? 1 : -1;
			int f1OPf2n = (f1.matches(newsgroups[i]) > 0
				       || f2.matches(newsgroups[i]) >
				       0) ? 1 : -1;

			if (f1OPf2n == f1OPf2m) {
				cout << "+" << flush;
			} else {
				cout << "-" << flush;
				ef++;
				V(cout << endl;
				  cout << "{" << f1 << "} and {" << f2 <<
				  "}=={" << f1OPf2 << "}" << endl;
				  cout << newsgroups[i] << flush;
				    );
			}
		}
		cout << " " << flush;
	}
};

template < class Check > void combine_filters(Check check)
{
	for (int i = 0; i < filters_n; i++) {
		NewsgroupFilter fi(filters[i]);
		cout << i << ": " << flush;
		for (int j = 0; j < filters_n; j++) {
			NewsgroupFilter fj(filters[j]);
			check(fi, fj);
		}
		cout << endl;
	}
}

int main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0)
		verbose = 1;
	cout << "cNewsgroupFilter:\n";

	cout << "operator&=:\n";
	combine_filters(CheckAND());

	cout << "operator|=:\n";
	combine_filters(CheckOR());

	if (ef == 0) {
		cout << "cNewsgroupFilter: all tests succeeded\n";
		exit(0);
	} else {
		cout << "cNewsgroupFilter: " << ef << " failures\n";
		exit(-1);
	}
}
