#ifndef _GroupInfo_h_
#define _GroupInfo_h_

#include <iostream>

#include "NSError.h"
#include "NVcontainer.h"

/**
 * \class GroupInfo
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class GroupInfo {
	nvtime_t _ctime;
	nvtime_t _mtime;
	unsigned int _fst, _lst, _n;
	char _flag;
	char _name[512];

      public:
	 GroupInfo() {
		init();
	} GroupInfo(const char *gd) {
		_name[sizeof(_name) - 1] = '\0';
		set(gd);
	}

	void init() {
		_ctime = _mtime = 0;
		_name[0] = _name[sizeof(_name) - 1] = '\0';
		_flag = 'y';
	}

	// All data-types stored in NVcontainers must have a set- and getraw
	// method
	void setraw(const char *buf, unsigned int bufsz) {
		if (bufsz > sizeof(_ctime) + sizeof(_mtime) +
		    sizeof(_fst) + sizeof(_lst) + sizeof(_n) +
		    sizeof(_flag) + sizeof(_name))
			throw Error("GroupInfo(24230): setraw buf too big",
				    ERROR_LOCATION);
		memcpy(this, buf, bufsz);
	}

	void getraw(const char **buf, unsigned int *bufsz) {
		*buf = (const char *) this;
		*bufsz = sizeof(_ctime) + sizeof(_mtime) +
		    sizeof(_fst) + sizeof(_lst) + sizeof(_n) +
		    sizeof(_flag) + strlen(_name) + 1;
	}

	void set(const char *gd) {
		const char *p = gd, *e;
		char *q = _name;
		unsigned int i = 0;

		while (i < sizeof(_name) && !isspace(*p)) {
			*q++ = *p++;
			++i;
		}
		if (i == sizeof(_name))
			throw Error("Name of newsgroup too long",
				    ERROR_LOCATION);
		*q = '\0';

		_lst = strtol(p, (char **) &e, 10);
		_fst = strtol(e, (char **) &p, 10);
		if (e == p)
			throw NSError("gd not in form name int int flag",
				      ERROR_LOCATION);

		if (_fst > _lst)
			_n = 0;
		else
			_n = _lst - _fst + 1;

		while (isspace(*p))
			p++;
		if (*p)
			_flag = *p;
		else
			_flag = 'y';

		nvtime(&_mtime);
	}

	void set(const char *group, int fst, int lst, int n, char fl = 'y') {
		strncpy(_name, group, sizeof(_name));
		if (_name[sizeof(_name) - 1] != '\0')
			throw Error("Name of newsgroup too long",
				    ERROR_LOCATION);
		setfln(fst, lst, n, fl);
	}

	void setfln(int first, int last, int n, char fl = 'y') {
		nvtime_t tm;
		_fst = first;
		_lst = last;
		_n = n;
		if (fl)
			_flag = fl;
		nvtime(&tm);
		_mtime = tm;
	}
	void setflag(char fl) {
		_flag = fl;
	}

	void setctime(nvtime_t ctime) {
		_ctime = ctime;
	}

	nvtime_t ctime() const {
		return _ctime;
	} nvtime_t mtime() const {
		return _mtime;
	} const char *name() const {
		return _name;
	} unsigned int first() const {
		return _fst;
	} unsigned int last() const {
		return _lst;
	} unsigned int n() const {
		return _n;
	} char flags() const {
		return _flag;
	} friend std::ostream & operator <<(std::ostream & os,
					    const GroupInfo & gd) {
		os << gd._name << ' ' << gd._lst << ' ' << gd.
		    _fst << ' ' << gd._flag;
		return os;
	}
};

#endif
