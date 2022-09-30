/* Copyright (C) 2003 Herbert Straub
 * Original Author (Article.h) Thomas Gschwind
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "Article.h"

#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif

using namespace std;

InvalidArticleError::InvalidArticleError(const char *txt, const char *file,
										 const char *function, int line)
	: NSError(txt, file, function, line)
{
}

InvalidArticleError::InvalidArticleError(const string & txt, const char *file,
										 const char *function, int line)
	: NSError(txt, file, function, line)
{
}

void InvalidArticleError::print(void) const
{
	slog << "Exception!\n"
	    << "  Type: InvalidArticle\n"
	    << "  Desc: " << _errtext << "\n";
}

Article::Article()
{
}

Article::Article(Article * a)
	: _nbr(a->getnbr()), _text(a->GetText()), _ctext(_text.c_str())
{ }

Article::Article(int artnbr)
{
	setnbr(artnbr);
}

Article::Article(int artnbr, const char *text, int textlen)
{
	setnbr(artnbr);
	_text.assign(text, textlen);
	_ctext = _text.c_str();
}

Article::~Article ()
{
}

void Article::read(std::istream & is)
{
	DEBUG(slog.p(Logger::Debug) << "Article::read(&is)\n");
	_text = "";
	readtext(is, _text);
	_ctext = _text.c_str();
}

void Article::setnbr(int nbr)
{
	_nbr = nbr;
}
	
void Article::clear(void)
{
	_nbr = 0;
	_text = "";
	_ctext = _text.c_str();
}

void Article::settext(const std::string & text)
{
	_text = text;
	_ctext = _text.c_str();
}

int Article::getnbr(void) const
{
	return _nbr;
}

string Article::GetText (void)
{
	return _text;
}	
	
const char* Article::c_str(void)
{
	return _ctext;
}
	
int Article::length(void) const
{
	return _text.length();
}
	
int Article::has_field(const char *ifld)
{
	char c;

	c = *find_field(ifld);
	if (!c || c == '\r' || c == '\n')
		return 0;
	return 1;
}

const char* Article::find_field(const char *ifld) const
{
	char hdrbuf[513], *p;
	const char *q, *fld;
	const char *ifp;
	unsigned int i;

	i = 1;
	p = hdrbuf;
	ifp = ifld;
	while (i < sizeof(hdrbuf) && (*p = tolower(*ifp))) {
		i++;
		p++;
		ifp++;
	}
	if (i == sizeof(hdrbuf)) {
		p--;
		VERB(slog.
		     p(Logger::
		       Error) <<
		     "Article::find_field: field exceeds 512 characters! Contact maintainer!\n");
	}
	*p = '\0';

	q = _ctext;
	for (;;) {
		p = hdrbuf;
		fld = q;
		while (*p && *p == tolower(*q)) {
			p++;
			q++;
		}
		if (*p == '\0')
			return fld;

		// bad luck find next line
		while (*q && *q != '\r' && *q != '\n')
			q++;
		if (!*q) {
			VERB(slog.
			     p(Logger::
			       Error) <<
			     "Article::find_field: unterminated header, no body\n");
			throw
			    InvalidArticleError
			    ("unterminated header, no body",
			     ERROR_LOCATION);
		}
		// Skip first CRLF
		if (*q == '\r' && *(q + 1) == '\n') {
			q += 2;
		} else if (*q == '\n') {
			++q;
		} else {
			VERB(slog.
			     p(Logger::
			       Error) <<
			     "Article::find_field: line not terminated with CRLF\n");
			throw
			    InvalidArticleError
			    ("line not terminated with CRLF/LF",
			     ERROR_LOCATION);
		}
		if (*q == '\r' || *q == '\n')
			return q;
	}
}

string Article::getfield(const char *ifld, int Full) const
{
	string rfld;
	const char *p, *fld;

	if (strcasecmp(ifld, "bytes:") == 0) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << _text.length();
		return sb.rdbuf()->str();
	}

	fld = find_field(ifld);
	if (*fld == '\r' || *fld == '\n') {
		// Bad luck, did not find the header
		throw NoSuchFieldError(ifld, ERROR_LOCATION);
	}
	// found header
	p = fld + strlen(ifld);
	while (*p == ' ' || *p == '\t')
		p++;
	if (!Full)
		fld = p;
	while (*p && *p != '\r' && *p != '\n')
		p++;
	rfld.assign(fld, p - fld);

	if (*p == '\n')
		p++;
	else if (*p == '\r') {
		if (*(p + 1) != '\n')
			throw
			    InvalidArticleError
			    ("illegaly terminated line, neither <lf> nor <cr><lf>",
			     ERROR_LOCATION);
		p += 2;
	}
	if (!(*p))
		throw InvalidArticleError("article without body",
					  ERROR_LOCATION);

	// multi-line header?
	while (*p == ' ' || *p == '\t') {
		do {
			p++;
		} while (*p == ' ' || *p == '\t');
		fld = p;
		while (*p && *p != '\r' && *p != '\n')
			p++;
		rfld.append(fld, p - fld);

		if (*p == '\n')
			p++;
		else if (*p == '\r') {
			if (*(p + 1) != '\n')
				throw
				    InvalidArticleError
				    ("illegaly terminated line, neither <lf> nor <cr><lf>",
				     ERROR_LOCATION);
			p += 2;
		}
		if (!(*p))
			throw
			    InvalidArticleError
			    ("article without body", ERROR_LOCATION);
	}
	return rfld;
}

void Article::setfield(const char *ifld, const char *field_data)
{
	const char *p, *fld;

	fld = find_field(ifld);
	if (*fld == '\r' || *fld == '\n') {
		// Bad luck, did not find the header
		_text.insert(fld - _ctext, field_data);
		_ctext = _text.c_str();
		return;
	}
	// We have found the header
	p = fld + strlen(ifld);
	while (isspace(*p))
		p++;
	while (*p && *p != '\r' && *p != '\n')
		p++;
	ASSERT(if (!(*p)) {
	       slog.p(Logger::Error) << "Article without body.\n";}
	       if (*p == '\r' && !(*(p + 1))) {
	       slog.p(Logger::Error) << "Article ended with <CR><NULL>.\n";}
	);
	if (*p == '\n')
		p++;
	if (*p == '\r' && *(p + 1) == '\n')
		p += 2;

	// Multiline header?
	while (isspace(*p) && *p != '\n' && *p != '\r') {
		while (isspace(*p))
			p++;
		while (*p != '\r' && *p != '\n')
			p++;
		ASSERT(if (!(*p)) {
		       slog.p(Logger::Error) << "Article without body.\n";}
		       if (*p == '\r' && !(*(p + 1))) {
		       slog.
		       p(Logger::
			 Error) << "Article ended with <CR><NULL>.\n";}
		);
		if (*p == '\n')
			p++;
		if (*p == '\r' && *(p + 1) == '\n')
			p += 2;
	}
	_text.replace(fld - _ctext, p - fld, field_data);
	_ctext = _text.c_str();
}

ostream & Article::write(ostream & os, int flags)
{
	const char *p, *q;

	if ((flags & (Head | Body)) == (Head | Body)) {
		os << _text;
		return os;
	}

	p = q = _ctext;
	for (;;) {
		while (*q && *q != '\r' && *q != '\n')
			q++;
		if (!*q) {
			VERB(slog.
			     p(Logger::
			       Notice) <<
			     "Article::write: Article without body!\n");
			break;
		}
		if (*q == '\r' && *(q + 1) == '\n'
		    && *(q + 2) == '\r' && *(q + 3) == '\n') {
			q += 2;
			break;
		}
		if (*q == '\n' && *(q + 1) == '\n') {
			q++;
			break;
		}
		while (*q == '\n' || *q == '\r')
			q++;
	}
	if (flags & Head) {
		os.write(p, q - p);
	} else if (flags & Body) {
		if (*q == '\n')
			p++;
		else if (*q == '\r' && *(q + 1) == '\n')
			q += 2;

		os.write(q, _text.length() - (q - p));
	}
	return os;
}

ostream & operator <<(ostream & os, Article & art)
{
	os << art._text;
	return os;
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
