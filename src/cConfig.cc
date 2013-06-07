#include <iostream>

#include "Logger.h"
#include "Config.h"

using namespace std;

Logger slog;
Config cfg;

int main(int argc, char *argv[])
{
	chdir(getenv("srcdir"));
	cfg.read("../etc/newscache.conf-dist");

	cout << "cConfig: newscache.conf-dist ok\n";
	exit(0);
}
