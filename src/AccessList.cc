#include "util.h"
#include "AccessList.h"
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>


#include <algorithm>

using namespace std;

std::ostream & operator<<(std::ostream & os, const AccessEntry & ae)
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
	std::string t = sb.str();
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

bool in_domain(const std::string &name, const std::string &domain)
{
	const size_t name_len = name.length();
	const size_t domain_len = domain.length();

	if (domain[0] == '.') {	/* suffix */
		if (name_len > domain_len) {
			const size_t n = name_len - domain_len;
			return STR_EQ(name.c_str() + n, domain.c_str());
		}
	} else {		/* exact match */
		return STR_EQ(name.c_str(), domain.c_str());
	}

	return false;
}

bool matchaddress(const struct sockaddr *pattern_addr,
		  socklen_t pattern_addrlen, unsigned short prefixlen,
		  const struct sockaddr *host_addr, socklen_t host_addrlen)
{
	const uint8_t *pattern_bytes = NULL;
	const uint8_t *host_bytes = NULL;
	size_t addrlen = 0;

	if ((pattern_addr->sa_family == AF_INET) &&
	    (host_addr->sa_family == AF_INET)) {
		addrlen = 4;
		pattern_bytes = (uint8_t *) &((const struct sockaddr_in *) pattern_addr)->sin_addr.s_addr;
		host_bytes = (uint8_t *) &((const struct sockaddr_in *) host_addr)->sin_addr.s_addr;
	}

	if ((pattern_addr->sa_family == AF_INET6) &&
	    (host_addr->sa_family == AF_INET6)) {
		addrlen = 16;
		pattern_bytes = ((const struct sockaddr_in6 *) pattern_addr)->sin6_addr.s6_addr;
		host_bytes = ((const struct sockaddr_in6 *) host_addr)->sin6_addr.s6_addr;
	}

	if ((pattern_addr->sa_family == AF_INET) &&
	    (host_addr->sa_family == AF_INET6) &&
	    (IN6_IS_ADDR_V4MAPPED(&((const struct sockaddr_in6 *) host_addr)->sin6_addr))) {
		addrlen = 4;
		pattern_bytes = (uint8_t *) &((const struct sockaddr_in *) pattern_addr)->sin_addr.s_addr;
		host_bytes = &((const struct sockaddr_in6 *) host_addr)->sin6_addr.s6_addr[12];
	}

	if ((pattern_bytes != NULL) && (host_bytes != NULL)) {
		for (unsigned int i = 0; i < addrlen; i++) {
			if (prefixlen >= (8*i + 8)) {
				if ((*pattern_bytes ^ *host_bytes) != 0) {
					return false;
				}
			} else {
				return (*pattern_bytes ^ *host_bytes) < (1 << (8 - (prefixlen % 8)));
			}

			pattern_bytes++;
			host_bytes++;
		}
	}

	return false;
}

AccessEntry *AccessList::client(const char *name, const struct sockaddr *addr,
				socklen_t addrlen)
{
	std::vector < AccessEntry >::iterator iter = vector.begin();
	const std::vector < AccessEntry >::iterator &end = vector.end();

	while (iter != end) {
		if (iter->addr.ss_family == AF_UNSPEC) {
			if (iter->hostname.empty() ||
			    in_domain(name, iter->hostname)) {
				return &(*iter);
			}
		} else {
			if (matchaddress((const struct sockaddr *) &iter->addr,
					 sizeof(iter->addr),
					 iter->prefixlen, addr, addrlen)) {
				return &(*iter);
			}
		}
		++iter;
	}
	return NULL;
}

void AccessList::read(Lexer & lex)
{
	std::string tok, host, port;
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

void AccessEntry::printParameters (std::ostream *pOut)
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

void AccessEntry::modifyAccessFlags (const std::string &flags)
{
	std::string t;
	std::string::size_type i=0, j;

	if (flags.length() == 0) {
		return;
	}

	do {
		if ((j=flags.find (",", i)) == std::string::npos) {
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
	std::string tok, a1, a2, a3;
	int filter_set = 0;
	AccessEntry cur;

	int family = AF_UNSPEC;
	const char *slash = strchr(address, '/');
	if (slash != NULL) {
		if (strspn(slash + 1, "0123456789") == strlen(slash + 1)) {
			cur.prefixlen = atoi(slash + 1);
		} else {
			unsigned char mask[4];
			if (inet_pton(AF_INET, slash + 1, &mask) != 1) {
				throw SyntaxError(lex, "unable to parse IPv4 netmask", ERROR_LOCATION);
			}

			unsigned char b = 0;
			for (unsigned int i = 0; i < sizeof(mask); i++) {
				if (mask[i] != 0xff) {
					b = mask[i];
					break;
				}
				cur.prefixlen += 8;
			}

			if ((b & 0xf0) == 0xf0) {
				cur.prefixlen += 4;
				b &= 0x0f;
			} else {
				b >>= 4;
			}

			if ((b & 0x0c) == 0x0c) {
				cur.prefixlen += 2;
				b &= 0x03;
			} else {
				b >>= 2;
			}

			if (b & 0x02) {
				cur.prefixlen++;
			}
		}

		cur.hostname.assign(address, slash);
	} else {
		cur.hostname = address;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICHOST;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *res;
	if (!getaddrinfo(cur.hostname.c_str(), NULL, &hints, &res)) {
		memcpy(&cur.addr, res->ai_addr, res->ai_addrlen);

		cur.hostname.erase();
		freeaddrinfo(res);
	}

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
			std::string t = lex.getToken();
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

void Authentication::set (const std::string &AuthString)
{
	std::string::size_type beg=0, end=0; 

	fields.clear();

	if (AuthString.length() == 0) {
		return;
	}

	do {
		if ((end=AuthString.find(":",beg)) == std::string::npos) {
			end=AuthString.length();
		}
		if (end>beg) {
			fields.push_back (AuthString.substr(beg, end-beg));
		} else {
			fields.push_back (std::string(""));
		}
		beg=++end;
	} while (end<AuthString.length());

	// empty field at the end of AuthString?
	if (AuthString[AuthString.length()-1] == ':') {
		fields.push_back (std::string(""));
	}
}

void Authentication::printParameters (std::ostream *pOut) const
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
	vector<std::string>::iterator sp, se;
	vector<std::string>::iterator dp, de;

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
				fields.push_back (std::string(""));
			dp++;
		} else {
			if (i>=start)
				(*dp) += (*sp);
			sp++; dp++;
		}
	}
}

int Authentication::typeEqual (const std::string &v)
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

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
