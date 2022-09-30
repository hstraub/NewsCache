#ifndef __readline_h__
#define __readline_h__

#include<string>

/**
 * \author Thomas Gschwind
 * \param is Input-Stream.
 * \param strg String to store line.
 * \param keepnl Keep newline (Default=1).
 */
int nlreadline(std::istream & is, std::string & strg, int keepnl = 1);

/**
 * \author Thomas Gschwind
 * \param is Input-Stream.
 * \param strg String to store the text.
 * \param keepnl Keep newlines in text (Default=1).
 * \param eot EndOfText Marker. (Default=".\\r\\n").
 */
int readtext(std::istream & is, std::string & strg, int keepnl = 1,
		char *eot = ".\r\n"
    );

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
