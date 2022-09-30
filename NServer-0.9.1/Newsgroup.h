#ifndef __Newsgroup_h__
#define __Newsgroup_h__

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
#include <string>

#include "config.h"
#include "Debug.h"
#include "OverviewFmt.h"

// A NNTP-command must not exceed 512 characters including the
// trailing \r\n. 512 chars is a good approx for the max length
// of a newsgroup
#define MAXGROUPNAMELEN 512
#define MAXNEWSGROUPNAMELEN MAXGROUPNAMELEN

class Newsgroup {
  protected:
	char _NewsgroupName[MAXGROUPNAMELEN + 1];
	OverviewFmt *_OverviewFormat;
	virtual Article *retrievearticle(unsigned int nbr) {
		return NULL;
      }

  public:
	Newsgroup(OverviewFmt * fmt, const char *name) 
	{
		_OverviewFormat = fmt;
		_NewsgroupName[MAXGROUPNAMELEN] = '\0';
		strncpy(_NewsgroupName, name, MAXGROUPNAMELEN);
		ASSERT(if (strlen(name) > MAXGROUPNAMELEN) {
		       slog.p(Logger::Error) <<
				   "Name of newsgroup too long - truncated\n";}
		);
	}
	virtual ~ Newsgroup();

	void setoverviewfmt(OverviewFmt * fmt) {
		_OverviewFormat = fmt;
	}
	OverviewFmt *getoverviewfmt() {
		return _OverviewFormat;
	}

#ifdef ENABLE_ASSERTIONS
	virtual void testdb(void) {
	}
#endif
	const char *name(void) {
		return _NewsgroupName;
	}
	virtual void getsize(unsigned int *f, unsigned int *l) = 0;
	virtual void setsize(unsigned int f, unsigned int l) = 0;
	virtual unsigned int firstnbr() = 0;
	virtual unsigned int lastnbr() = 0;
	virtual int hasrecord(unsigned int i) = 0;
//   virtual void hasrecords(unsigned int *rlist)=0;

	virtual Article *getarticle(unsigned int nbr) = 0;
	virtual void freearticle(Article * artp) = 0;
	virtual void setarticle(Article * art) = 0;
	virtual void printarticle(std::ostream & os, unsigned int nbr) = 0;
	virtual void prefetchGroup(int lockgrp = 1) = 0;
	virtual void prefetchOverview(void) = 0;

	virtual const char *getover(unsigned int nbr) = 0;
	virtual void setover(const std::string & over) = 0;
	virtual void printover(std::ostream & os, unsigned int nbr) {
		const char *o = getover(nbr);
		if (o)
			os << o << "\r\n";
	}
	virtual void readoverdb(std::istream & is) = 0;
	virtual void printoverdb(std::ostream & os, unsigned int f =
				 0, unsigned int l = UINT_MAX) {
		unsigned int i, ol;
		const char *o;
		getsize(&i, &ol);
		if (f > i)
			i = f;
		if (l > ol)
			l = ol;
		while (i <= l) {
			if ((o = getover(i++)) != NULL)
				os << o << "\r\n";
		}
	}
	virtual void printheaderdb(std::ostream & os,
							   const char *header,
							   unsigned int f = 0, unsigned int l =
							   UINT_MAX) = 0;
	virtual void printlistgroup(std::ostream & os) = 0;
};

#ifdef _INCLUDE_NewsgroupIter_
class NewsgroupIter {
  protected:
	Newsgroup * _ng;
	unsigned int i;
	int lck;

  public:
	NewsgroupIter()
	{ }

	NewsgroupIter(Newsgroup * ng) {
		_ng = ng;
	}

	void attach(Newsgroup * ng) {
		_ng = ng;
	}
	void first() {
		i++;
	}
	void next() {
		i--;
	}
	void set(unsigned int j) {
		i = j;
	}

	Article *article() {
		return _ng->getarticle(i);
	}
	OverviewRecord *overview() {
		return _ng->getarticle(i);
	}
};
#endif
#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
