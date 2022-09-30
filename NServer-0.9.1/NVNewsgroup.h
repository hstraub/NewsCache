#ifndef __NVNewsgroup_h__
#define __NVNewsgroup_h__

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

#include <iostream>
#include <fstream>
#include <string>

#include "config.h"
#include "Debug.h"
#include "util.h"
#include "NVArray.h"
#include "Newsgroup.h"

/**
 * \class NVNewsgroup
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class NVNewsgroup:public Newsgroup, protected NVArray {
  protected:
	void sprintover(std::ostream & os, unsigned int nbr);

	enum {
		hasarticle = 0x1,
		overview = 0x2,
		article = 0x3,
		bigarticle = 0x4
	};

	char _SpoolDirectory[MAXPATHLEN];
  public:
	NVNewsgroup(OverviewFmt * fmt, const char *spooldir,
				const char *name)
	: Newsgroup(fmt, name), NVArray()
	{
		char fn[MAXPATHLEN];
		sprintf(_SpoolDirectory, "%s/%s", spooldir,
				group2dir(name));
		mkpdir(_SpoolDirectory, 0755);
		sprintf(fn, "%s/.db", _SpoolDirectory);
		NVArray::open(fn);
	}

#ifdef ENABLE_ASSERTIONS
	virtual void testdb(void);
#endif
	virtual void getsize(unsigned int *f, unsigned int *l) {
		*f = arrfst;
		*l = arrlst;
	}
	virtual void setsize(unsigned int f, unsigned int l);
	virtual unsigned int firstnbr() {
		return arrfst;
	}
	virtual unsigned int lastnbr() {
		return arrlst;
	}
	virtual int hasrecord(unsigned int i) {
		return NVArray::has_element(i);
	}

	virtual Article *getarticle(unsigned int nbr);
	virtual void freearticle(Article * artp) {
		delete artp;
	}
	virtual void setarticle(Article * art);
	virtual void printarticle(std::ostream & os, unsigned int nbr);
	virtual void prefetchGroup(int lockgrp = 1) {
	}
	virtual void prefetchOverview(void) {
	}

	virtual const char *getover(unsigned int nbr);
	virtual void setover(const std::string & over);
	virtual void printover(std::ostream & os, unsigned int nbr) {
		NVArray::lock(NVcontainer::ShrdLock);
		sprintover(os, nbr);
		NVArray::lock(NVcontainer::UnLock);
	}
	virtual void readoverdb(std::istream & is);
	virtual void printoverdb(std::ostream & os, unsigned int f =
				 0, unsigned int l = UINT_MAX);
	virtual void printheaderdb(std::ostream & os, const char *header,
				   unsigned int f = 0, unsigned int l =
				   UINT_MAX);

	virtual void printlistgroup(std::ostream & os);
};


/* No Iterator needed at this point, because the Newsgroup iterator 
 * may also be used for a NVNewsgroup 
 */

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
