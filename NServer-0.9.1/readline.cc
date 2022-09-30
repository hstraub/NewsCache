#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>

#include "Debug.h"
#include "readline.h"

using namespace std;

int nlreadline(istream & is,	// Input-Stream
	       string & strg,	// String to store line
	       int keepnl	// Keep newline
    )
{
	int pos, len;
	try {
		getline(is, strg);
	} catch(...) {
		// FIXME: should be improved or removed (Debugging)
		slog.p(Logger::Error) << "nlreadline: Error in getline\n";
		std::exit(1);
	}
	pos = len = strg.length();
	while (pos) {
		pos--;
		if (strg[pos] != '\r' && strg[pos] != '\n') {
			pos++;
			break;
		}
	}
	if (keepnl) {
		strg.replace(pos, len - pos, "\r\n");
	} else {
		strg.replace(pos, len - pos, (char *) NULL, 0);
	}
	return strg.length();
}

int readtext(istream & is,	// Input-Stream
	     string & strg,	// String to store line
	     int keepnl,	// Keep newlines in text
	     char *eot		// EndOfText Marker
    )
{
	int lines = 0;
	string line;

	for (;;) {
		nlreadline(is, line, keepnl);
		if (line == eot)
			break;
		if (!is.good()) {
			if (is.eof())
				break;
			break;
		}
		strg += line;
		lines++;
	}

	return lines;
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
