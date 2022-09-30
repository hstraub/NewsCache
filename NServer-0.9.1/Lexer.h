#ifndef _Lexer_h_
#define _Lexer_h_
#include <ctype.h>
#include <stdio.h>

#include <iostream>
#include <fstream>

#include "Error.h"
#include "readline.h"

/**
 * \class Lexer
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class Lexer {
	friend class SyntaxError;
      private:
	const char *_fn;
	 std::ifstream _is;

	std::string _tok;

	std::string _buf;
	int _line;
	const char *_cbuf;
	const char *_cbufp;
      public:
	 Lexer(const char *fn = NULL) {
		_fn = NULL;
		if (fn)
			open(fn);
	} void close() {
		if (!_fn)
			return;
		_fn = NULL;
		_is.close();
	}

	void open(const char *fn) {
		_is.open(fn);
		if (!_is.good())
			throw IOError("Stream bad", -1, ERROR_LOCATION);
		_fn = fn;
		_line = 0;
		_buf = "";
		_cbufp = _cbuf = _buf.c_str();
	}

	int eof() {
		return _is.eof();
	}

	std::string curToken() {
		return _tok;
	}

	std::string getToken() {
		const char *q;
		while (!(*_cbufp)) {
			if (!_is.good())
				throw IOError("Stream bad", -1,
					      ERROR_LOCATION);
			_line++;
			nlreadline(_is, _buf, 0);
			if (_is.eof())
				return "";
			if (!_is.good())
				throw IOError("Stream bad", -1,
					      ERROR_LOCATION);
			_cbufp = _cbuf = _buf.c_str();
			while (isspace(*_cbufp))
				_cbufp++;
			if (*_cbufp == '#')
				_cbufp = _cbuf + _buf.length();
		}
		q = _cbufp;
		while (*q && !isspace(*q))
			q++;
		_tok.assign(_cbufp, q - _cbufp);
		_cbufp = q;
		while (isspace(*_cbufp))
			_cbufp++;
		return _tok;
	}

	void putbackToken(std::string token) {
		_buf.replace(0, _cbufp - _cbuf, token + ' ');
		_cbufp = _cbuf = _buf.c_str();
	}

	int isFlag(const std::string & token, const char *strg, int *flag) {
		const char *ctok = token.c_str();
		if (strcmp(ctok, strg) == 0) {
			*flag = 1;
			return 1;
		}
		if (strncmp(ctok, "not-", 4) == 0
		    && strcmp(ctok + 4, strg) == 0) {
			*flag = 0;
			return 1;
		}
		return 0;
	}
};

/**
 * \class SyntaxError
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class SyntaxError:public Error {
      public:
	SyntaxError(const Lexer & lex, const char *txt, const char *file,
		    const char *function, int line)
	:Error(txt, file, function, line) {
		char buf[256];
		 sprintf(buf, ":%d: ", lex._line);

		 _errtext = lex._fn;
		 _errtext += buf;
		 _errtext += txt;
		 VERB(slog.p(Logger::Error);
		      SyntaxError::print());
	}
	virtual void print() const {
		slog << "Exception!\n"
		    << "  Type: Syntax\n"
		    << "  Desc: " << _errtext << "\n";
	}
};

#endif
