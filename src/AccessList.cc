#include "util.h"
#include "AccessList.h"
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif


#include <algorithm>

using namespace std;

ostream & operator<<(ostream & os, const AccessEntry & ae)
{
#ifdef HAVE_SSTREAM
	stringstream sb;
#else
	strstream sb;
#endif

	os << ae.hostname << '{' << endl;
	os << "  access_flags=" << ae.access_flags << " (";
	ae.printAccessFlags(&os);
	os << ")"<< endl;
	os << "  list=" << ae.list << endl;
	os << "  read=" << ae.read << endl;
	os << "  postto=" << ae.postTo << endl;
	ae.authentication.printParameters (&sb);
	os << "  PAMServicename " << ae.PAMServicename << endl;
	string t = sb.str();
	// replace \t\tAuthentication with ""
	t.replace(0, 17, "");
	os << "  authentication=" << t << endl;
	os << '}';

	return os;
}

#define STRN_EQ(x,y,l)	(strncasecmp((x),(y),(l)) == 0)
#define STRN_NE(x,y,l)	(strncasecmp((x),(y),(l)) != 0)
#define STR_EQ(x,y)	(strcasecmp((x),(y)) == 0)
#define STR_NE(x,y)	(strcasecmp((x),(y)) != 0)


#define NOT_INADDR(s) (s[strspn(s,"01234567890./")] != 0)

int in_domain(const char *name, const char *domain)
{
	int name_len = strlen(name);
	int domain_len = strlen(domain);

	if (domain[domain_len - 1] == '.') {	/* prefix */
		return STRN_EQ(name, domain, domain_len);
	} else if (domain[0] == '.') {	/* suffix */
		int n = name_len - domain_len;
		return n > 0 && STR_EQ(name + n, domain);
	} else {		/* exact match */
		return STR_EQ(name, domain);
	}
}

int matchaddress(const char *pattern,
		 const char *name, const struct in_addr &host_addr)
{
	const char *p = pattern;

	while (*p && *p != '/')
		++p;

	if (*p) {
		string net(pattern, p - pattern);
		string mask(p + 1);

		struct in_addr net_addr, mask_addr;
		if (!inet_aton(net.c_str(), &net_addr))
			return 0;
		if (!inet_aton(mask.c_str(), &mask_addr))
			return 0;

		return (host_addr.s_addr & mask_addr.s_addr) ==
		    net_addr.s_addr;
	} else {		/* anything else */
		return NOT_INADDR(name) && in_domain(name, pattern);
	}
}

AccessEntry *AccessList::client(const char *name, struct in_addr addr)
{
	std::vector < AccessEntry >::iterator begin = vector.begin(), end =
	    vector.end();

	while (begin != end) {
		if (matchaddress(begin->hostname, name, addr) ||
		    begin->hostname[0] == '\0') {
			return &(*begin);
		}
		++begin;
	}
	return NULL;
}

void AccessList::read(Lexer & lex)
{
	string tok, host, port;
	int default_found = 0;
	tok = lex.getToken();
	if (tok != "{")
		throw SyntaxError(lex, "expected '{'", ERROR_LOCATION);
	for (;;) {
		tok = lex.getToken();
		if (tok == "Client") {
			host = lex.getToken();
			//port=lex.getToken();
			readClient(lex, host.c_str());
		} else if (tok == "Default") {
			readClient(lex, "");
			default_found++;
		} else if (tok == "}") {
			break;
		} else {
			throw SyntaxError(lex,
					  "expected declaration or '}'",
					  ERROR_LOCATION);
		}
	}
	if (!default_found)
		throw SyntaxError(lex,
				  "'Default' entry not found in NewsClientList",
				  ERROR_LOCATION);
}

#define ACCESSLIST_SET_FILTERS \
      if (!(filter_set&0x01)) cur.list=a1; \
      if (!(filter_set&0x02)) cur.read=a1; \
      if (!(filter_set&0x04)) cur.postTo=a1;

void AccessEntry::printParameters (ostream *pOut)
{
	if (hostname[0] != '\0') {
		*pOut << "\tClient " << hostname << " {" << endl;
	} else {
		*pOut << "\tDefault {" << endl;
	}
	*pOut << "\t\tallow";
	printAccessFlags (pOut);
	*pOut << endl;
	if (list.getRulelist() != "") {
		*pOut << "\t\tList " << list.getRulelist() << endl;
	}
	if (read.getRulelist() != "") {
		*pOut << "\t\tRead " << read.getRulelist() << endl;
	}
	if (postTo.getRulelist() != "") {
		*pOut << "\t\tPostTo " << postTo.getRulelist() << endl;
	}
	authentication.printParameters (pOut);
	*pOut << "\t\tPAMServicename " << PAMServicename << endl;
	*pOut << "\t}" << endl;
}

void AccessEntry::modifyAccessFlags (const string &flags)
{
	string t;
	string::size_type i=0, j;

	if (flags.length() == 0) {
		return;
	}

	do {
		if ((j=flags.find (",", i)) == string::npos) {
			j=flags.length();
		}
		t = flags.substr (i, j-i);
		slog.p(Logger::Debug) << "j: " << (unsigned int)j << " flags: "
			<< flags << " t: " << t << "\t";
		if (t == "read") {
			access_flags |= af_read;
		} else if (t == "!read") {
			access_flags |= ~af_read;
		} else if (t == "post") {
			access_flags |= af_post;
		} else if (t == "!post") {
			access_flags |= ~af_post;
		} else if (t == "debug") {
			access_flags |= af_debug;
		} else if (t == "!debug") {
			access_flags |= ~af_debug;
		}
		i=++j;
	} while (j<flags.length());

}


void AccessList::readClient(Lexer & lex, const char *address)
{
	string tok, a1, a2, a3;
	int filter_set = 0;
	AccessEntry cur;
	strcpy(cur.hostname, address);

	tok = lex.getToken();
	if (tok != "{")
		throw SyntaxError(lex, "expected '{'", ERROR_LOCATION);
	for (;;) {
		tok = lex.getToken();
		if (tok == "allow") {
			for (;;) {
				a1 = lex.getToken();
				if (a1 == "read")
					cur.access_flags |=
					    AccessEntry::af_read;
				else if (a1 == "post")
					cur.access_flags |=
					    AccessEntry::af_post;
				else if (a1 == "debug")
					cur.access_flags |=
					    AccessEntry::af_debug;
				else if (a1 == "authentication")
					cur.access_flags |=
					    AccessEntry::af_authentication;
				else if (a1 == "none")
					cur.access_flags = 0x0;
				else
					break;
			}
			lex.putbackToken(a1);
		} else if (tok == "List") {
			a1 = lex.getToken();
			cur.list = a1;
			filter_set |= 0x01;
			ACCESSLIST_SET_FILTERS;
		} else if (tok == "Read") {
			cur.read = a1 = lex.getToken();
			filter_set |= 0x02;
			ACCESSLIST_SET_FILTERS;
		} else if (tok == "PostTo") {
			cur.postTo = a1 = lex.getToken();
			filter_set |= 0x04;
			ACCESSLIST_SET_FILTERS;
		} else if (tok == "Authentication") {
			string t = lex.getToken();
			cur.authentication.set (t);
		} else if (tok == "PAMServicename") {
			cur.PAMServicename = lex.getToken();
		} else if (tok == "}") {
			break;
		} else {
			throw SyntaxError(lex,
					  "expected declaration or '}'",
					  ERROR_LOCATION);
		}
	}

	vector.push_back(cur);
}

void AccessList::printParameters (std::ostream *pOut)
{
	std::vector<AccessEntry>::iterator begin, end;

	*pOut << "AccessList {" << endl;

	for (begin=vector.begin(), end=vector.end();
			begin != end; begin++) {
		begin->printParameters (pOut);
	}

	*pOut << "}" << endl;
}

void Authentication::set (const string &AuthString)
{
	string::size_type beg=0, end=0; 

	fields.clear();

	if (AuthString.length() == 0) {
		return;
	}

	do {
		if ((end=AuthString.find(":",beg)) == string::npos) {
			end=AuthString.length();
		}
		if (end>beg) {
			fields.push_back (AuthString.substr(beg, end-beg));
		} else {
			fields.push_back (string(""));
		}
		beg=++end;
	} while (end<AuthString.length());

	// empty field at the end of AuthString?
	if (AuthString[AuthString.length()-1] == ':') {
		fields.push_back (string(""));
	}
}

void Authentication::printParameters (ostream *pOut) const
{
	int i,e=fields.size();

	if (e == 0) {
		return;
	}

	for (i=0; i<e; i++) {
		if (i==0) {
			*pOut << "\t\tAuthentication ";
		} else {
			*pOut << ":";
		}
		*pOut << fields[i];
	}
	*pOut << endl;
}

void Authentication::modify (unsigned int start, Authentication &source)
{
	unsigned int i;
	vector<string>::iterator sp, se;
	vector<string>::iterator dp, de;

	sp = source.fields.begin();
	se = source.fields.end();
	dp = fields.begin();
	de = fields.end();

	for (i=0; sp != se && dp != de; i++) {
		if (dp == de) {
			if (i>=start)
				fields.push_back (*sp);
			sp++;
		} else if (sp == se) {
			if (i>=start)
				fields.push_back (string(""));
			dp++;
		} else {
			if (i>=start)
				(*dp) += (*sp);
			sp++; dp++;
		}
	}
}

int Authentication::typeEqual (const string &v)
{
	const char *s = fields[0].c_str();
	const char *p = v.c_str();

	while (*s!='\0') {
		if (*p=='\0')
			return -1;
		if (*s!=*p)
			return -1;
		s++; p++;
	}
	if (*p=='\0' || *p==':')
		return 0;
	else 
		return -1;

}
