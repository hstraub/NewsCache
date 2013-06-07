#ifndef _NVHash_h_
#define _NVHash_h_

#include"NVlist.h"

/* NVContainer Class Library */
class NVHash;
class NVHashIter;

/**
 * \class NVHash
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
class NVHash:public NVlist {
	friend class NVHashIter;

      protected:
	void make_current(void);
	nvoff_t *hashtab;
	unsigned long hashsz;

	int sis_empty(void);
	void sclear(void);
	void sadd(unsigned long h, const char *data, size_t szdata);
	void sprint(std::ostream & os);

      public:
	 NVHash():NVlist() {
	} NVHash(const char *dbname, unsigned long hashsz = 97, int flags =
		 0);

	/**
	 * Open an NVHash object.
	 * \param dbname Name of the file, where the hash-table is stored.
	 * \param hashsz Size of the hash-table, if a new one is created.
	 * \param flags Ignored at the moment.
	 */
	void open(const char *dbname, unsigned long hashsz =
		  97, int flags = 0);

	/**
	 * Returns the size of the has-table.
	 * \return size of the hash table
	 */
	unsigned long gethashsz(void) {
		// No lock and make_current invocation necessary, since 
		// the hash-size must not change
		return hashsz;
	}

	void clear(void);
	int is_empty(void);
	void add(unsigned long h, const char *data, size_t szdata);

	void print(std::ostream & os);
};

/**
 * \class NVHashIter
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
class NVHashIter {
	NVHash *ht;
	 NVHash::Record * pos;
	 NVHash::Record * curtail;
	unsigned long curhashval;
      public:
	 NVHashIter() {
		ht = NULL;
		pos = NULL;
	} NVHashIter(NVHash & nvl) {
		ht = &nvl;
		ht->lock(NVHash::ShrdLock);
		pos = NULL;
	}
	~NVHashIter() {
		detach();
	}

	void attach(NVHash & nvl) {
		detach();
		ht = &nvl;
		ht->lock(NVHash::ShrdLock);
		pos = NULL;
	}
	void detach() {
		if (ht) {
			ht->lock(NVHash::UnLock);
		}
		ht = NULL;
	}

	void first() {
		pos = NULL;
		curhashval = 0;
		while (curhashval < ht->hashsz) {
			if (ht->hashtab[curhashval]) {
				curtail = ht->o2r(ht->hashtab[curhashval]);
				pos = ht->o2r(curtail->next);
				break;
			}
			curhashval++;
		}
	}
	int valid() {
		return pos != NULL;
	}
	void next() {
		if (pos) {
			if (pos == curtail) {
				pos = NULL;
				curhashval++;
				while (curhashval < ht->hashsz) {
					if (ht->hashtab[curhashval]) {
						curtail =
						    ht->o2r(ht->
							    hashtab
							    [curhashval]);
						pos =
						    ht->o2r(curtail->next);
						break;
					}
					curhashval++;
				}
			} else {
				pos = ht->o2r(pos->next);
			}
		}
	}

	void data(const char **data, unsigned int *szdata) {
		(*data) =
		    ht->mem_p + ht->r2o(pos) + sizeof(NVHash::Record);
		(*szdata) = pos->szdata;
	}

};

#endif
