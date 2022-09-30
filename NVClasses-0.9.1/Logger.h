#ifndef _Logger_h_
#define _Logger_h_

#include <iostream>
#include <fstream>
#include <string>

#include "config.h"

#ifndef HAVE_SYSLOG_H
#undef WITH_SYSLOG
#endif

#ifdef WITH_SYSLOG
#include<syslog.h>
#endif

/**
 * \class Logger
 * \author Thomas Gschwind
 * Levels:
 * 	\li Debug ...... Debugging Only
 *	\li Info ....... Informations, eg to trace the program
 *	\li Notice ..... More improtant notices
 *	\li Warning .... Something strange
 *	\li Error ...... Something unexpected
 *	\li Critical ... Something did not succeed, critical
 *	\li Alert ...... Action must be taken immediately
 *	\li Emergency .. System is unusable
 */
class Logger {

#ifndef WITH_SYSLOG
	std::ofstream log;
#endif

	int _priority;

	char *buf;
	int buflen, bufsz;

	int bufresize(int sz);
	void append(const char *s);
	void print();

      public:
#ifdef WITH_SYSLOG
	enum { Emergency = LOG_EMERG,
		Alert = LOG_ALERT,
		Critical = LOG_CRIT,
		Error = LOG_ERR,
		Warning = LOG_WARNING,
		Notice = LOG_NOTICE,
		Info = LOG_INFO,
		Debug = LOG_DEBUG
	};
	enum { Cons = LOG_CONS,
		NDelay = LOG_NDELAY,
#ifdef LOG_PERROR
		PError = LOG_PERROR,
#else
		PError = 0,
#endif
#ifdef LOG_NOWAIT
		NoWait = LOG_NOWAIT,
#else
		NoWait = 0,
#endif
		Pid = LOG_PID
	};
#else
	enum { Emergency = 0,
		Alert = 1,
		Critical = 2,
		Error = 3,
		Warning = 4,
		Notice = 5,
		Info = 6,
		Debug = 7
	};
#endif

#ifdef WITH_SYSLOG
	Logger();
	void open(char *ident, int option, int facility);
#else
	Logger(char *fn = NULL, int option = 0);
	void open(char *fn);
#endif
	~Logger();

	void close();

	Logger & priority(int p);
	Logger & p(int p) {
		return priority(p);
	} Logger & write(const char *s);
	Logger & write(char ch) {
		char buf[2];
		buf[0] = ch;
		buf[1] = '\0';
		return write(buf);
	}

	friend Logger & operator<<(Logger & l, const char *s);
	friend Logger & operator<<(Logger & l, const std::string & s);
	friend Logger & operator<<(Logger & l, char ch);
	friend Logger & operator<<(Logger & l, unsigned int i);
	friend Logger & operator<<(Logger & l, int i);
};

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
