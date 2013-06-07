#include "util.h"
#include "MPList.h"

//#include <vector>
//#include <algorithm>

using namespace std;

MPListEntry *MPList::postserver(const char *group)
{
	unsigned int i;
	// bm=Best Match
	int bm = -1, bmlen = 0, clen;

	for (i = 0; i < entries.size(); i++) {
		clen = matchgroup(entries[i].postTo, group);
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
		clen = matchgroup(entries[i].read, group);
		if (clen > bmlen) {
			bm = i;
			bmlen = clen;
		}
	}
	if (bm >= 0 && entries[bm].hostname[0] != '\0')
		return &(entries[bm]);
	return NULL;
}

void MPListEntry::printParameters (ostream *pOut)
{
	if (hostname[0] != '\0') {
		*pOut << "\tServer " << hostname << " " << servicename
			<< " {" << endl;
	} else {
		*pOut << "\tNoServer " << " {" << endl;
	}
	if (user[0] != '\0') {
		*pOut << "\t\tUser " << user << endl;
	}
	if (passwd[0] != '\0') {
		*pOut << "\t\tPassword " << passwd << endl;
	}
	if (read[0] != '\0') {
		*pOut << "\t\tRead "<< read << endl;
	}
	if (postTo[0] != '\0') {
		*pOut << "\t\tPostTo " << postTo << endl;
	}
	if (bindFrom[0] != '\0') {
		*pOut << "\t\tBindFrom " << bindFrom << endl;
	}
	*pOut << "\t\tGroupTimeout " << groupTimeout << endl;
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
	*pOut << endl;
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
		*pOut << endl;
	}
	*pOut << "\t\tRetries " << retries << endl;
	*pOut << "\t}" << endl;
}

void MPList::read(Lexer & lex)
{
	string tok, host, port;

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
	string tok, a1, a2, a3;
	int havePostTo = 0;
	MPListEntry cur;

	if (host) {
		strcpy(cur.hostname, host);
		strcpy(cur.servicename, port);
	}

	tok = lex.getToken();
	if (tok != "{")
		throw SyntaxError(lex, "expected '{'", ERROR_LOCATION);
	for (;;) {
		tok = lex.getToken();
		if (tok == "Read") {
			a1 = lex.getToken();
			if (a1.length() >= sizeof(cur.read))
				throw SyntaxError(lex,
						  "group description too long",
						  ERROR_LOCATION);
			strcpy(cur.read, a1.c_str());
			if (!havePostTo) {
				if (a1.length() >= sizeof(cur.postTo))
					throw SyntaxError(lex,
							  "group description too long",
							  ERROR_LOCATION);
				strcpy(cur.postTo, a1.c_str());
			}
		} else if (tok == "BindFrom") {
			a1 = lex.getToken();
			if (a1.length() >= sizeof(cur.bindFrom))
				throw SyntaxError(lex,
						  "BindFrom field is too long",
						  ERROR_LOCATION);
			strcpy(cur.bindFrom, a1.c_str());
		} else if (tok == "PostTo") {
			a1 = lex.getToken();
			if (a1.length() >= sizeof(cur.postTo))
				throw SyntaxError(lex,
						  "group description too long",
						  ERROR_LOCATION);
			strcpy(cur.postTo, a1.c_str());
		} else if (tok == "GroupTimeout") {
			a1 = lex.getToken();
			cur.groupTimeout = atoi(a1.c_str());
		} else if (tok == "Retries") {
			a1 = lex.getToken();
			cur.retries = atoi(a1.c_str());
		} else if (tok == "User") {
			a1 = lex.getToken();
			strncpy(cur.user, a1.c_str(), 64);
		} else if (tok == "Password") {
			a1 = lex.getToken();
			strncpy(cur.passwd, a1.c_str(), 64);
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

void MPList::printParameters (ostream *pOut)
{
	vector<MPListEntry>::iterator begin, end;

	*pOut << "NewsServerList {" << endl;
	
	
	for (begin=entries.begin(), end=entries.end();
			begin != end; begin++) {
		begin->printParameters (pOut);
	}

	*pOut << "}" << endl;
}
