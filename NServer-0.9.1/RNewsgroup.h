#ifndef __RNewsgroup_h__
#define __RNewsgroup_h__

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "config.h"
#include "Debug.h"
#include "OverviewFmt.h"
#include "Newsgroup.h"
#include "NServer.h"

/**
 * \class RNewsgroup
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
class RNewsgroup:public Newsgroup {
	typedef std::map < unsigned int, string >::iterator iterator;

	 std::map < unsigned int, string > _OverviewDB;
	unsigned int _first, _last;

	// News stuff
	RServer *_RemoteServer;

      public:
	 RNewsgroup():Newsgroup(NULL, NULL) {
	} RNewsgroup(RServer * srvr, OverviewFmt * fmt, const char *name)
	:Newsgroup(fmt, name), _first(1), _last(0), _RemoteServer(srvr) {
	}
	~RNewsgroup() {
	}

	virtual void getsize(unsigned int *f, unsigned int *l) {
		*f = _first;
		*l = _last;
	}

	virtual void setsize(unsigned int f, unsigned int l);

	virtual void prefetchGroup(int lockgrp = 1) {
	}
	virtual void prefetchOverview(void) {
	}

	virtual unsigned int firstnbr() {
		return _first;
	}
	virtual unsigned int lastnbr() {
		return _last;
	}
	virtual int hasrecord(unsigned int i) {
		return (_OverviewDB.find(i) != _OverviewDB.end());
	}

	virtual Article *getarticle(unsigned int nbr) {
		try {
			Article *a = new Article;
			_RemoteServer->article(_NewsgroupName, nbr, a);
			return a;
		}
		catch(...) {
			return NULL;
		}
	}

	virtual void freearticle(Article * artp) {
		delete artp;
	}
	virtual void setarticle(Article * art) {
	}
	virtual void printarticle(std::ostream & os, unsigned int nbr);

	virtual const char *getover(unsigned int nbr) {
		iterator over = _OverviewDB.find(nbr);
		if (over != _OverviewDB.end())
			return over->second.c_str();
		else
			return NULL;
	}

	virtual void setover(const string & over) {
		unsigned int i = atoi(over.c_str());

		if (_first <= i && i <= _last) {
			_OverviewDB[i] = over;
		}
	}

	virtual void readoverdb(std::istream & is) {
		VERB(slog.
		     p(Logger::Debug) << "RNewsgroup::readoverdb(&is)\n");
		string line1, line2;

		for (;;) {
			nlreadline(is, line1, 0);
			if (line1 == "." || is.eof())
				break;
			if (_OverviewFormat->dotrans) {
				// Convert Record
				_OverviewFormat->convert(line1, line2);
				setover(line2);
			} else {
				setover(line1);
			}
		}
	}

	virtual void printheaderdb(ostream & os,
				   const char *header,
				   unsigned int f = 0, unsigned int l =
				   UINT_MAX) {
		char xheader[512], *p;
		const char *q;
		string fld;

		p = xheader;
		q = header;
		while ((*p++ = *q++));
		*(p - 1) = ':';
		*p = '\0';

		iterator begin = _OverviewDB.lower_bound(f), end =
		    _OverviewDB.end();

		while (begin != end) {
			unsigned int i = begin->first;
			if (l < i)
				break;

			try {
				fld =
				    _OverviewFormat->
				    getfield(_OverviewDB[i].c_str(),
					     xheader, 0);
				os << i << " " << fld << "\r\n";
			}
			catch(NoSuchFieldError & nsfe) {
				os << i << " (none)\r\n";
			}
			++begin;
		}
	}

	virtual void printlistgroup(ostream & os) {
		iterator begin = _OverviewDB.begin(), end =
		    _OverviewDB.end();

		while (begin != end) {
			os << begin->first << "\r\n";
			++begin;
		}
	}
};
#endif
