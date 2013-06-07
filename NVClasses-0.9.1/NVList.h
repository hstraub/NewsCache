#ifndef _NVList_h_
#define _NVList_h_

#include"NVlist.h"

/* NVContainer Class Library */
class NVList;
class NVListIter;

/**
 * \class NVList
 * \author Thomas Gschwind
 * \bug Documentation missing
 */
class NVList:public NVlist {
	friend class NVListIter;

      public:
	 NVList():NVlist() {
	} NVList(const char *dbname, int flags = 0):NVlist(dbname, flags) {
	}

	int is_empty(void);
	void prepend(const char *data, size_t szdata);
	void append(const char *data, size_t szdata);
	void remove(void);
	void clear(void);

	void print(std::ostream & os);
};

/**
 * \class NVListIter
 * \author Thomas Gschwind
 * \bug Documentation missing
 */
class NVListIter {
	NVList *l;
	 NVList::Record * pos;
	 NVList::Record * tail;
      public:
	 NVListIter() {
		l = NULL;
		pos = NULL;
	} NVListIter(NVList & nvl) {
		l = &nvl;
		l->lock(NVList::ShrdLock);
		pos = NULL;
	}
	~NVListIter() {
		detach();
	}

	void attach(NVList & nvl) {
		detach();
		l = &nvl;
		l->lock(NVList::ShrdLock);
		pos = NULL;
		tail = l->o2r(l->getdata());
	}
	void detach() {
		if (l) {
			l->lock(NVList::UnLock);
		}
		l = NULL;
	}

	void first() {
		if (l)
			pos = l->o2r(tail->next);
	}
	int valid() {
		return pos != NULL;
	}
	void next() {
		if (pos) {
			if (pos == tail)
				pos = 0;
			else
				pos = l->o2r(pos->next);
		}
	}

	void data(const char **data, unsigned int *szdata) {
		(*data) = l->mem_p + l->r2o(pos) + sizeof(NVList::Record);
		(*szdata) = pos->szdata;
	}
};

#endif
