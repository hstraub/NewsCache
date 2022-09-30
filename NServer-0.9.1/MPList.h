#ifndef _MPList_h_
#define _MPList_h_
#include <ctype.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>
#include <string>
#include <vector>

#include "Debug.h"
#include "Error.h"
#include "OverviewFmt.h"
#include "Lexer.h"

struct MPListEntry {
      public:
	enum {
		F_SETPOSTFLAG = 0x01,
		F_CACHED = 0x02,
		F_OFFLINE = 0x04,
		F_SEMIOFFLINE = 0x08,
		F_DONTGENMSGID = 0x10
	};
	// nntp commands supported by the server
	enum {
		F_LIST_ACTIVE_WILDMAT = 0x01,
		F_LIST_OVERVIEW_FMT = 0x02,
		F_LISTGROUP = 0x04,
		F_MODE_READER = 0x08,
		F_OVER = 0x10,
		F_XOVER = 0x20,
		F_POST = 0x40
	};

	std::string hostname;
	std::string servicename;
	std::string user;
	std::string passwd;

	OverviewFmt overview;

	std::string read;	// Groups read from that server
	std::string postTo;	// Postings handled by this server
	struct sockaddr_storage bindFrom;	// From which interface

	unsigned int groupTimeout;
	unsigned int flags;	// configuration flags
	unsigned int nntpflags;	// supported NNTP-Commands
	unsigned int limitgroupsize; // limit the size of a group
	unsigned int retries;
	unsigned int connectBackoff;
	unsigned int connectTimeout;

	mutable time_t connectFailed;

	MPListEntry();
	void init(void);
	void printParameters (std::ostream *pOut);
};

/**
 * \class MPList
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class MPList {
      public:
	std::vector < MPListEntry > entries;

      private:

      public:
	MPList() {};
//   void addserver(const char *ns,const char *p,const char *g) {//     if(e_used==e_alloc) myrealloc(e_used+1);//     entries[e_used].init(ns,p);//     if(g) {//       strcpy(entries[e_used].read,g);//       strcpy(entries[e_used].postTo,g);//     }//     e_used++;//   }
	std::string makeFilter(unsigned int servernbr,
			       const char *listarg) const;
	MPListEntry *server(const char *group);
	MPListEntry *postserver(const char *group);
	void init(void);
	void read(Lexer & lex);
	void readServer(Lexer & lex, const char *host, const char *port);
	void printParameters(std::ostream *pOut);

};


// Inline methods
inline MPListEntry::MPListEntry () {
	init();
}

inline void MPListEntry::init(void)
{
	bindFrom.ss_family = AF_UNSPEC;
	groupTimeout = 600;	// 10m
	limitgroupsize = 0;
	retries = 3;

	connectBackoff = 0;
	connectTimeout = 0;
	connectFailed = 0;

	flags = F_SETPOSTFLAG | F_CACHED;
	nntpflags = 0xffffffff;
}

inline std::string MPList::makeFilter(unsigned int servernbr, const char *listarg) const
{
	std::string filter;

	if (strcmp(listarg, "*") == 0) {
		filter = entries[servernbr].read;
		for (unsigned int i = 0; i < entries.size(); i++) {
			if (i != servernbr) {
				std::string::const_iterator p = entries[i].read.begin();
				const std::string::const_iterator end = entries[i].read.end();
				std::string::const_iterator q;
				for (;;) {
					q = p;
					char c;
					while (p != end && (c = *p) != ',')
						++p;
					filter += ",!";
					filter.append(q, p);
					if (p == end)
						break;
					++p;
				}
			}
		}
	} else {
		unsigned int i;
		filter = "*";
		if (servernbr == 0)
			i = 1;
		else
			i = 0;
		const char *listp = listarg;
		char c = '\0';
		std::string::const_iterator p, q;
		std::string::const_iterator end;
		while (i < entries.size()) {
			if (!c) {
				q = p = entries[i].read.begin();
				end = entries[i].read.end();
			}
			while ((c = *listp) && p != end && c == *p) {
				listp++;
				++p;
			}
			if (*listp == '*') {
				// p is matched by listarg
				ASSERT(if (p != end && *p == '*') {
				       slog.
				       p(Logger::
					 Error) <<
				       "Same newsgroup expression configured for two different servers!\n";}
				);
				filter += ",!";
				while (p != end && (c = *p) != ',')
					++p;
				filter.append(q, p);
			} else {
				// p is not matched by listarg
				while (p != end && (c = *p) != ',')
					++p;
			}
			if (!c) {
				i++;
				if (servernbr == i)
					i++;
			} else {
				q = ++p;
			}
		}
	}

	return filter;
}

inline void MPList::init(void)
{
	entries.clear();
}

#endif
