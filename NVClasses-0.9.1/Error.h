#ifndef _Error_h_
#define _Error_h_

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <string>

#include "Debug.h"
#include "Logger.h"

#ifndef _ERROR_LOCATION_
#define _ERROR_LOCATION_
#define ERROR_LOCATION __FILE__, __PRETTY_FUNCTION__, __LINE__
#endif

/*
 * 2002-10-17 Herbert Straub: reformatting the print method output (one line)
 * 2002-10-29 Herbert Straub: adding ERROR_LOCATION, _errtext, _file, _function
 * 	Modify all constructers add ERROR_LOCATION
 * 	Class Error Constructor: add VERB for debug output
 */

extern Logger slog;

/**
 * \class Error
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class Error {
      public:
	string _errtext;
	string _file;
	string _function;
	int _line;
	 Error(const char *txt = "unknown"):_errtext(txt) {
		VERB(slog.p(Logger::Debug);
		     print());
	} Error(const char *txt, const char *file, const char *function,
		int line)
	:_errtext(txt), _file(file), _function(function), _line(line) {
		VERB(slog.p(Logger::Debug);
		     print());
	}
	Error(const string & txt):_errtext(txt) {
		VERB(slog.p(Logger::Debug);
		     print());
	}
	Error(const string & txt, const char *file, const char *function,
	      int line)
	:_errtext(txt), _file(file), _function(function), _line(line) {
		VERB(slog.p(Logger::Debug);
		     print());
	}
	virtual ~ Error() {
	}

	virtual void print() {
		slog << "Exception!"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class SystemError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class SystemError:public Error {
      public:
	int _errno;
	 SystemError(const char *txt = "unknown", int errnbr = -1)
	:Error(txt), _errno(errnbr) {
	} SystemError(const string & txt, int errnbr = -1)
	:Error(txt), _errno(errnbr) {
	}
	SystemError(const char *txt, int errnbr, const char *file,
		    const char *function, int line)
	:Error(txt, file, function, line), _errno(errnbr) {
	}
	SystemError(const string & txt, int errnbr, const char *file,
		    const char *function, int line)
	:Error(txt, file, function, line), _errno(errnbr) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: System"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line
		    << " Desc: " << _errtext
		    << " ErrStr(" << _errno << "): " << strerror(_errno) <<
		    "\n";
	}
};

/**
 * \class IOError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class IOError:public SystemError {
      public:
	IOError(const char *txt = "unknown", int errnbr =
		-1):SystemError(txt, errnbr) {
	} IOError(const char *txt, int errnbr, const char *file,
		  const char *function, int line):SystemError(txt, errnbr,
							      file,
							      function,
							      line) {
	}
	IOError(const string & txt, int errnbr =
		-1):SystemError(txt, errnbr) {
	}
	IOError(const string & txt, int errnbr, const char *file,
		const char *function, int line):SystemError(txt, errnbr,
							    file, function,
							    line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: IO"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line
		    << " Desc: " << _errtext
		    << " ErrStr(" << _errno << "): " << strerror(_errno) <<
		    "\n";
	}
};

/**
 * \class AssertionError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class AssertionError:public Error {
      public:
	AssertionError(const char *txt):Error(txt) {
	} AssertionError(const char *txt, const char *file,
			 const char *function, int line):Error(txt, file,
							       function,
							       line) {
	}
	AssertionError(const string & txt):Error(txt) {
	}
	AssertionError(const string & txt, const char *file,
		       const char *function, int line):Error(txt, file,
							     function,
							     line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: Assertion"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

#endif
