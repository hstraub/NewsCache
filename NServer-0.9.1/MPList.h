#ifndef _MPList_h_
#define _MPList_h_
#include <ctype.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/param.h>

#include <iostream>
#include <fstream>
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
		F_SEMIOFFLINE = 0x08
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

	char hostname[MAXHOSTNAMELEN];
	char servicename[256];
	char user[64];
	char passwd[64];

	OverviewFmt overview;

	char read[2048];	// Groups read from that server
	char postTo[2048];	// Postings handled by this server
	char bindFrom[2048];	// From which interface
	time_t groupTimeout;

	unsigned int flags;	// configuration flags
	unsigned int nntpflags;	// supported NNTP-Commands
	int retries;

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
	const char *makeFilter(unsigned int servernbr,
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
	hostname[0] = servicename[0] = '\0';
	user[0] = passwd[0] = '\0';

	read[0] = postTo[0] = bindFrom[0] = '\0';
	groupTimeout = 600;	// 10m
	retries = 3;

	flags = F_SETPOSTFLAG | F_CACHED;
	nntpflags = 0xffffffff;
}

inline const char *MPList::makeFilter(unsigned int servernbr, const char *listarg) const
{
	static string filter;
	char c;
	unsigned int i;

	if (strcmp(listarg, "*") == 0) {
		const char *p, *q;
		filter = entries[servernbr].read;
		for (i = 0; i < entries.size(); i++) {
			if (i != servernbr) {
				p = entries[i].read;
				for (;;) {
					q = p;
					while ((c = *p) != ',' && c)
						p++;
					filter += ",!";
					filter.append(q, p - q);
					if (!c)
						break;
					p++;
				}
			}
		}
	} else {
		const char *listp, *p = NULL, *q = NULL;
		filter = "*";
		if (servernbr == 0)
			i = 1;
		else
			i = 0;
		listp = listarg;
		c = '\0';
		while (i < entries.size()) {
			if (!c) {
				q = p = entries[i].read;
			}
			while ((c = *listp) && c == *p) {
				listp++;
				p++;
			}
			if (*listp == '*') {
				// p is matched by listarg
				ASSERT(if (*p == '*') {
				       slog.
				       p(Logger::
					 Error) <<
				       "Same newsgroup expression configured for two different servers!\n";}
				);
				filter += ",!";
				while ((c = *p) != ',' && c)
					p++;
				filter.append(q, p - q);
			} else {
				// p is not matched by listarg
				while ((c = *p) != ',' && c)
					p++;
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

	return filter.c_str();
}

inline void MPList::init(void)
{
	entries.clear();
}

#endif
