#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <string>

#include "util.h"
#include "NSError.h"
#include "NVActiveDB.h"
#include "readline.h"

using namespace std;

// protected:
unsigned long NVActiveDB::hash(const char *strg)
{
	unsigned long h = 1;
	const char *p = strg;
	while (p[0]) {
		h = (h * (p[0]) + p[1]) % NVActiveDB_HASHSIZE;
		if (!p[1])
			break;
		p += 2;
	}
	return h;
}

void NVActiveDB::sset(GroupInfo & gi, int flags)
{
	//   GroupInfo cg;
	const char *gidata;
	unsigned int gisz;
	Record *head = o2r(hashtab[hash(gi.name())]);
	Record *r = head;

	while (r) {
		r = o2r(r->next);

		// The first approach is the cleaner one since it uses 
		// GroupInfo's setraw function. However, this involves 
		// a copy operation
//     cg.setraw(r->datap(),r->szdata);
//     if(strcmp(gi.name(),cg.name())==0) {
//       gi.setctime(cg.ctime());
		// The second approach assumes that the GroupInfo
		// data is stored in the same layout in the file.
		// The last (sizeof(GroupInfo)-r->szdata) bytes don't
		// matter. These are junk anyways.
		if (strcmp(((GroupInfo *) r->datap())->name(), gi.name())
		    == 0) {
			gi.setctime(((GroupInfo *) r->datap())->ctime());
			gi.getraw(&gidata, &gisz);
			if (gisz != r->szdata) {
				VERB(slog.
				     p(Logger::
				       Error) <<
				     "NVActiveDB::set: Size of new record not equal original record size. Should have been impossible.\n");
			} else {
				memcpy(r->datap(), gidata, gisz);
			}
			break;
		}
		if (r == head)
			r = NULL;
	}

	if (!r) {
		if (flags & update_only) {
			// Should we throw an exception here?!?
			VERB(slog.
			     p(Logger::
			       Error) <<
			     "NVActiveDB::set: Cannot find requested record\n");
		} else {
			const char *buf;
			unsigned int bufsz;
			nvtime_t now;
			nvtime(&now);
			gi.setctime(now);
			gi.getraw(&buf, &bufsz);
			NVHash::add(hash(gi.name()), buf, bufsz);
		}
	}
}

void NVActiveDB::add(GroupInfo & gi)
{
	VERB(slog.p(Logger::Debug) << "NVActiveDB::add(GroupInfo &gi)\n)");
	const char *buf;
	unsigned int bufsz;
	gi.getraw(&buf, &bufsz);
	NVHash::add(hash(gi.name()), buf, bufsz);
}

int NVActiveDB::get(const char *group, GroupInfo * gi)
{
	VERB(slog.
	     p(Logger::Debug) << "NVActiveDB::get(" << group << ",&gi)\n");
	lock(ShrdLock);
	Record *head = o2r(hashtab[hash(group)]);
	Record *r = head;

	while (r) {
		r = o2r(r->next);

		// The first approach is the cleaner one since it uses 
		// GroupInfo's setraw function. However, this involves 
		// a copy operation
//     gi->setraw(r->datap(),r->szdata);
//     if(strcmp(gi->name(),group)==0) break;

		// The second approach assumes that the GroupInfo
		// data is stored in the same layout in the file.
		// The last (sizeof(GroupInfo)-r->szdata) bytes don't
		// matter. These are junk anyways.
		if (strcmp(((GroupInfo *) r->datap())->name(), group) == 0) {
			gi->setraw(r->datap(), r->szdata);
			break;
		}
		if (r == head)
			r = NULL;
	}
	lock(UnLock);

	if (!r)
		return -1;
	return 0;
}

int NVActiveDB::hasgroup(const char *group)
{
	VERB(slog.
	     p(Logger::Info) << "NVActiveDB::hasgroup(" << group << ")\n");
	lock(ShrdLock);
	Record *head = o2r(hashtab[hash(group)]);
	Record *r = head;

	while (r) {
		r = o2r(r->next);

		// The first approach is the cleaner one since it uses 
		// GroupInfo's setraw function. However, this involves 
		// a copy operation
//     gi->setraw(r->datap(),r->szdata);
//     if(strcmp(gi->name(),group)==0) break;

		// The second approach assumes that the GroupInfo
		// data is stored in the same layout in the file.
		// The last (sizeof(GroupInfo)-r->szdata) bytes don't
		// matter. These are junk anyways.
		if (strcmp(((GroupInfo *) r->datap())->name(), group) == 0) {
			break;
		}
		if (r == head)
			r = NULL;
	}
	lock(UnLock);

	if (!r)
		return 0;
	return 1;
}

void NVActiveDB::read(istream & is, const char *filter, int flags)
{
	VERB(slog.
	     p(Logger::Debug) << "NVActiveDB::read(&is,*filter,flags)\n");

	GroupInfo gd;
	string line;
	time_t now;

	lock(NVcontainer::ExclLock);
	if (flags & F_CLEAR)
		clear();
	while (!is.eof()) {
		// Each line has the following form
		// GroupName Last First Flags
		nlreadline(is, line, 0);
		if (line == ".")
			break;

		if (!filter || matchgroup(filter, line.c_str()) > 0) {
			// Add group
			gd.set(line.c_str());
			if (flags & F_STORE_READONLY)
				gd.setflag('n');
			sset(gd);
		}
	}
	time(&now);
	setmtime(now);
	lock(NVcontainer::UnLock);
}

void NVActiveDB::write(ostream & os,
		       nvtime_t ctime, int mode, const char *filter)
{
	VERB(slog.
	     p(Logger::Debug) << "NVActiveDB::write(&os,ctime,...)\n");

	NVHashIter nvhi(*this);
	GroupInfo gd;
	const char *buf;
	unsigned int bufsz;

	for (nvhi.first(); nvhi.valid(); nvhi.next()) {
		nvhi.data(&buf, &bufsz);
		gd.setraw(buf, bufsz);
		if (((!filter) ||
		     ((matchgroup(filter, gd.name()) > 0))) &&
		    ctime < gd.ctime()) {
			if (mode == m_active)
				os << gd << "\r\n";
			else
				os << gd.name() << " " << gd.
				    ctime() << " news\r\n";
			if (!os.good()) {
				throw SystemError("Cannot write GroupList",
						  errno, ERROR_LOCATION);
			}
		}
	}			/* for */
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
