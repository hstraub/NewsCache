#ifndef _NSError_h_
#define _NSError_h_

#include <stdlib.h>
#include <errno.h>

#include <string>

#include "Debug.h"
#include "Logger.h"
#include "Error.h"

/*
 * 2002-10-17 Herbert Straub: reformatting the print method output (one line)
 * 2002-10-22 Herbert Straub: changing NSError::print() to print(), because it
 * 	produces a wrong Type Information in the logfile.
 * 2002-10-29 Herbert Straub: moving VERB in contructor to class Error
 */

/* NSError: ErrorCodes 
 */

/**
 * \class NSError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NSError:public Error {
      public:
	NSError(const char *txt = "unknown"):Error(txt) {
	} NSError(const char *txt, const char *file, const char *function,
		  int line):Error(txt, file, function, line) {
	}
	NSError(const string & txt):Error(txt) {
	}
	NSError(const string & txt, const char *file, const char *function,
		int line):Error(txt, file, function, line) {
	}
	virtual ~ NSError() {
	}

	virtual void print() {
		slog << "Exception! "
		    << " Type: NServer"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class NoSuchFieldError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NoSuchFieldError:public NSError {
      public:
	NoSuchFieldError(const char *txt):NSError(txt) {
	} NoSuchFieldError(const char *txt, const char *file,
			   const char *function, int line):NSError(txt,
								   file,
								   function,
								   line) {
	}
	NoSuchFieldError(const string & txt):NSError(txt) {
	}
	NoSuchFieldError(const string & txt, const char *file,
			 const char *function, int line):NSError(txt, file,
								 function,
								 line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: NotFound"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class NoNewsServerError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NoNewsServerError:public NSError {
      public:
	NoNewsServerError(const char *txt = "unknown"):NSError(txt) {
	} NoNewsServerError(const char *txt, const char *file,
			    const char *function, int line):NSError(txt,
								    file,
								    function,
								    line) {
	}
	NoNewsServerError(const string & txt):NSError(txt) {
	}
	NoNewsServerError(const string & txt, const char *file,
			  const char *function, int line):NSError(txt,
								  file,
								  function,
								  line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: NoNewsServer"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class NoSuchGroupError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NoSuchGroupError:public NSError {
      public:
	NoSuchGroupError(const char *txt = "unknown"):NSError(txt) {
	} NoSuchGroupError(const char *txt, const char *file,
			   const char *function, int line):NSError(txt,
								   file,
								   function,
								   line) {
	}
	NoSuchGroupError(const string & txt):NSError(txt) {
	}
	NoSuchGroupError(const string & txt, const char *file,
			 const char *function, int line):NSError(txt, file,
								 function,
								 line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: NoSuchGroup"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class DuplicateArticleError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class DuplicateArticleError:public NSError {
      public:
	DuplicateArticleError(const char *txt = "unknown"):NSError(txt) {
	} DuplicateArticleError(const char *txt, const char *file,
				const char *function,
				int line):NSError(txt, file, function,
						  line) {
	}
	DuplicateArticleError(const string & txt):NSError(txt) {
	}
	DuplicateArticleError(const string & txt, const char *file,
			      const char *function, int line):NSError(txt,
								      file,
								      function,
								      line)
	{
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: DuplicateArticle"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class NoSuchArticleError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NoSuchArticleError:public NSError {
      public:
	NoSuchArticleError(const char *txt = "unknown"):NSError(txt) {
	} NoSuchArticleError(const char *txt, const char *file,
			     const char *function, int line):NSError(txt,
								     file,
								     function,
								     line)
	{
	}
	NoSuchArticleError(const string & txt):NSError(txt) {
	}
	NoSuchArticleError(const string & txt, const char *file,
			   const char *function, int line):NSError(txt,
								   file,
								   function,
								   line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: NoSuchArticle"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class PostingFailedError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class PostingFailedError:public NSError {
      public:
	PostingFailedError(const char *txt = "unknown"):NSError(txt) {
	} PostingFailedError(const char *txt, const char *file,
			     const char *function, int line):NSError(txt,
								     file,
								     function,
								     line)
	{
	}
	PostingFailedError(const string & txt):NSError(txt) {
	}
	PostingFailedError(const string & txt, const char *file,
			   const char *function, int line):NSError(txt,
								   file,
								   function,
								   line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: PostingFailedError"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

class NotAllowedError:public NSError {
      public:
	NotAllowedError(const char *txt = "unknown"):NSError(txt) {
	} NotAllowedError(const char *txt, const char *file,
			  const char *function, int line):NSError(txt,
								  file,
								  function,
								  line) {
	}
	NotAllowedError(const string & txt):NSError(txt) {
	}
	NotAllowedError(const string & txt, const char *file,
			const char *function, int line):NSError(txt, file,
								function,
								line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: NoSuchArticle"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};

/**
 * \class UsageError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class UsageError:public NSError {
      public:
	UsageError(const char *txt = "unknown"):NSError(txt) {
	} UsageError(const char *txt, const char *file,
		     const char *function, int line):NSError(txt, file,
							     function,
							     line) {
	}
	UsageError(const string & txt):NSError(txt) {
	}
	UsageError(const string & txt, const char *file,
		   const char *function, int line):NSError(txt, file,
							   function,
							   line) {
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: Usage"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line << " Desc: " << _errtext << "\n";
	}
};
typedef UsageError UsageErr;

/**
 * \class ResponseError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class ResponseError:public NSError {
      public:
	string _command;
	string _expected;
	string _got;

	 ResponseError() {
	} ResponseError(const char *command, const char *exp,
			const char *got)
	:_command(command), _expected(exp), _got(got) {
		slog.p(Logger::Error);
		ResponseError::print();
	}
	ResponseError(const char *command, const char *exp,
		      const string & got)
	:_command(command), _expected(exp), _got(got) {
		slog.p(Logger::Error);
		ResponseError::print();
	}
	ResponseError(const string & command, const string & exp,
		      const string & got)
	:_command(command), _expected(exp), _got(got) {
		slog.p(Logger::Error);
		ResponseError::print();
	}

	virtual void print() {
		slog << "Exception!"
		    << " Type: Response"
		    << " File: " << _file
		    << " Function: " << _function
		    << " Line: " << _line
		    << " Cmd : " << _command
		    << " Exp : " << _expected << " Got : " << _got << "\n";
	}
};
typedef ResponseError ResponseErr;

#endif
