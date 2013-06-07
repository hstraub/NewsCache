#include <iostream>
#include <fstream>
#include <string>

#include <stdio.h>
#include <string.h>

#include "NSError.h"

using namespace std;

Logger slog;

int test(int f)
{
	try {
		throw NoSuchGroupError("no such group");
	} catch(Error & e) {
		e.print();
		if (f)
			return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int ef = 0;

	cout << "cNSError:\n";
	if (test(0) != 0) {
		cout << "test1 failed\n";
		ef++;
	}
	if (test(1) != -1) {
		cout << "test1 failed\n";
		ef++;
	}

	if (ef == 0) {
		cout << "cNSError: all tests succeeded\n";
		exit(0);
	} else {
		cout << "cNSError: " << ef << " tests failed\n";
		exit(-1);
	}
}
