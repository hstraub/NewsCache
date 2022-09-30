#include "util.h"
#include "MPList.h"

#include <netinet/in.h>

#include <algorithm>
#include <iostream>
#include <vector>

MPListEntry *MPList::postserver(const char *group)
{
	unsigned int i;
	// bm=Best Match
	int bm = -1, bmlen = 0, clen;

	for (i = 0; i < entries.size(); i++) {
		clen = matchgroup(entries[i].postTo.c_str(), group);
		if (clen > bmlen) {
			bm = i;
			bmlen = clen;
		}
	}
	if (bm >= 0 && entries[bm].hostname[0] != '\0')
		return &(entries[bm]);
	return NULL;
}

MPListEntry *MPList::server(const char *group)
{
	unsigned int i;
	// bm=Best Match
	int bm = -1, bmlen = 0, clen;

	for (i = 0; i < entries.size(); i++) {
		clen = matchgroup(entries[i].read.c_str(), group);
		if (clen > bmlen) {
			bm = i;
			bmlen = clen;
		}
	}
	if (bm >= 0 && entries[bm].hostname[0] != '\0')
		return &(entries[bm]);
	return NULL;
}

void MPListEntry::printParameters (std::ostream *pOut)
{
	if (hostname[0] != '\0') {
		*pOut << "\tServer " << hostname << " " << servicename
			<< " {" << std::endl;
	} else {
		*pOut << "\tNoServer " << " {" << std::endl;
	}
	if (user[0] != '\0') {
		*pOut << "\t\tUser " << user << std::endl;
	}
	if (passwd[0] != '\0') {
		*pOut << "\t\tPassword " << passwd << std::endl;
	}
	if (read[0] != '\0') {
		*pOut << "\t\tRead "<< read << std::endl;
	}
	if (postTo[0] != '\0') {
		*pOut << "\t\tPostTo " << postTo << std::endl;
	}
	*pOut << "\t\tGroupTimeout " << groupTimeout << std::endl;
	*pOut << "\t\tOptions";
	if (flags & F_SETPOSTFLAG) {
		*pOut << " setpostflag";
	}
	if (flags & F_CACHED) {
		*pOut << " cached";
	}
	if (flags & F_OFFLINE) {
		*pOut << " offline";
	}
	if (flags & F_SEMIOFFLINE) {
		*pOut << " semioffline";
	}
	*pOut << std::endl;
	if (~nntpflags) {
		*pOut << "\t\tCommands";
		if ((~nntpflags) & F_LIST_ACTIVE_WILDMAT) {
			*pOut << " list_active_wildmat";
		}
		if ((~nntpflags) & F_LIST_OVERVIEW_FMT) {
			*pOut << " list_overview_fmt";
		}
		if ((~nntpflags) & F_LISTGROUP) {
			*pOut << " not-listgroup";
		}
		if ((~nntpflags) & F_MODE_READER) {
			*pOut << " not-mode_reader";
		}
		if ((~nntpflags) & F_OVER) {
			*pOut << " not-over";
		}
		if ((~nntpflags) & F_XOVER) {
			*pOut << " not-xover";
		}
		if ((~nntpflags) & F_POST) {
			*pOut << " not-post";
		}
		*pOut << std::endl;
	}
	*pOut << "\t\tLimitGroupSize " << limitgroupsize << std::endl;
	*pOut << "\t\tRetries " << retries << std::endl;
	*pOut << "\t\tConnectBackoff " << connectBackoff << std::endl;
	*pOut << "\t\tConnectTimeout " << connectTimeout << std::endl;
	*pOut << "\t}" << std::endl;
}

void MPList::read(Lexer & lex)
{
	std::string tok, host, port;

	tok = lex.getToken();
	if (tok != "{")
		throw SyntaxError(lex, "expected '{'", ERROR_LOCATION);
	for (;;) {
		tok = lex.getToken();
		if (tok == "Server") {
			host = lex.getToken();
			port = lex.getToken();
			readServer(lex, host.c_str(), port.c_str());
		} else if (tok == "NoServer") {
			readServer(lex, NULL, NULL);
		} else if (tok == "}") {
			break;
		} else {
			throw SyntaxError(lex,
					  "expected declaration or '}'",
					  ERROR_LOCATION);
		}
	}
}

void MPList::readServer(Lexer & lex, const char *host, const char *port)
{
	std::string tok, a1, a2, a3;
	int havePostTo = 0;
	MPListEntry cur;

	if (host) {
		cur.hostname = host;
		cur.servicename = port;
	}

	tok = lex.getToken();
	if (tok != "{")
		throw SyntaxError(lex, "expected '{'", ERROR_LOCATION);
	for (;;) {
		tok = lex.getToken();
		if (tok == "Read") {
			a1 = lex.getToken();
			cur.read = a1;
			if (!havePostTo) {
				cur.postTo = a1;
			}
		} else if (tok == "BindFrom") {
			a1 = lex.getToken();
			struct addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE | AI_NUMERICHOST;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			struct addrinfo *res;
			if (getaddrinfo(a1.c_str(), NULL, &hints, &res)) {
				throw SyntaxError(lex,
						  "unable to resolve BindFrom",
						  ERROR_LOCATION);
			}

			memcpy(&cur.bindFrom, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		} else if (tok == "PostTo") {
			a1 = lex.getToken();
			cur.postTo = a1;
		} else if (tok == "GroupTimeout") {
			a1 = lex.getToken();
			cur.groupTimeout = atoi(a1.c_str());
		} else if (tok == "LimitGroupSize") {
			a1 = lex.getToken();
			cur.limitgroupsize = atoi(a1.c_str());
		} else if (tok == "Retries") {
			a1 = lex.getToken();
			cur.retries = atoi(a1.c_str());
		} else if (tok == "ConnectBackoff") {
			a1 = lex.getToken();
			cur.connectBackoff = atoi(a1.c_str());
		} else if (tok == "ConnectTimeout") {
			a1 = lex.getToken();
			cur.connectTimeout = atoi(a1.c_str());
		} else if (tok == "User") {
			a1 = lex.getToken();
			cur.user = a1;
		} else if (tok == "Password") {
			a1 = lex.getToken();
			cur.passwd = a1;
		} else if (tok == "Options") {
			int flag;
			for (;;) {
				a1 = lex.getToken();
				if (lex.
				    isFlag(a1.c_str(), "setpostflag",
					   &flag)) {
					if (flag)
						cur.flags |=
						    MPListEntry::
						    F_SETPOSTFLAG;
					else
						cur.flags &=
						    ~MPListEntry::
						    F_SETPOSTFLAG;
				} else if (lex.
					   isFlag(a1.c_str(), "cached",
						  &flag)) {
					if (flag)
						cur.flags |=
						    MPListEntry::F_CACHED;
					else
						cur.flags &=
						    ~MPListEntry::F_CACHED;
				} else if (lex.
					   isFlag(a1.c_str(), "offline",
						  &flag)) {
					if (flag)
						cur.flags |=
						    MPListEntry::F_OFFLINE;
					else
						cur.flags &=
						    ~MPListEntry::
						    F_OFFLINE;
				} else if (a1 == "semioffline") {
					cur.flags |=
					    MPListEntry::F_SEMIOFFLINE;
				} else if(a1=="dontgenmsgid") {
					cur.flags |=
					    MPListEntry::F_DONTGENMSGID;
				} else {
					break;
				}
			}
			lex.putbackToken(a1);
		} else if (tok == "Commands") {
			int flag;
			for (;;) {
				a1 = lex.getToken();
				if (lex.
				    isFlag(a1.c_str(), "listgroup",
					   &flag)) {
					if (flag)
						cur.nntpflags |=
						    MPListEntry::
						    F_LISTGROUP;
					else
						cur.nntpflags &=
						    ~MPListEntry::
						    F_LISTGROUP;
				} else if (lex.
					   isFlag(a1.c_str(), "over",
						  &flag)) {
					if (flag)
						cur.nntpflags |=
						    MPListEntry::F_OVER;
					else
						cur.nntpflags &=
						    ~MPListEntry::F_OVER;
				} else {
					break;
				}
			}
			lex.putbackToken(a1);
		} else if (tok == "}") {
			break;
		} else {
			throw SyntaxError(lex,
					  "expected declaration or '}'",
					  ERROR_LOCATION);
		}
	}

	entries.push_back(cur);
}

void MPList::printParameters (std::ostream *pOut)
{
	std::vector<MPListEntry>::iterator begin, end;

	*pOut << "NewsServerList {" << std::endl;
	
	
	for (begin=entries.begin(), end=entries.end();
			begin != end; begin++) {
		begin->printParameters (pOut);
	}

	*pOut << "}" << std::endl;
}
