#ifndef _NVArray_h_
#define _NVArray_h_

#include<iostream>

#include"NVcontainer.h"

/* NVContainer Class Library */
class NVArray;
class NVArrayIter;

/**
 * \class NVArray
 * \author Thomas Gschwind
 * \bug Documentation is missing.
 */
class NVArray:public NVcontainer {
  protected:
	void make_current(void);
	nvoff_t *arrtab;
	unsigned long arrfst, arrlst;

	void sclear(void);
	void sset(unsigned long i, const char *data, size_t szdata);
	void sdel(unsigned long i);
	int sis_empty(void);
	int shas_element(unsigned long i);
	void sget(unsigned long i, char **data, size_t * szdata);
	void sprint(std::ostream & os);

  public:
	enum {
		force = 0x1
	};
	NVArray():NVcontainer() {
	} NVArray(const char *dbname, int flags = 0);

	ASSERT(nvoff_t nvalloc(size_t rsz); void nvfree(nvoff_t p);)

	/**
	 * Open an NVArray container
	 * \param dbname name of the file, where the array is stored.
	 * \param flags Ignored at the moment.
	 */
	void open(const char *dbname, int flags = 0);

	void ssetsize(unsigned long fst, unsigned long lst);

	/**
	 * Sets a new size for the NVArray.
	 * \param fst index of first element.
	 * \param lst index of last element.
	 * \param flags indicates, whether the new size must be set even
	 * 	if some elements will not be accessible any more.
	 * 	Those elements will be removed.
	 * \return
	 * 	\li 0 Success
	 * 	\li -1 Error
	 */
	int setsize(unsigned long fst, unsigned long lst, int flags = 0);

	/**
	 * Returns the size of NVArray.
	 * \param fst Index of first element.
	 * \param lst Index of last element.
	 */
	void getsize(unsigned long *fst, unsigned long *lst) {
		// No lock necessary, because the user has to lock the NVArray
		// himself, if he wants to have an accurate information.
		*fst = arrfst;
		*lst = arrlst;
	}

	void clear(void);
	void set(unsigned long i, const char *data, size_t szdata);
	void del(unsigned long i);
	int is_empty(void);
	int has_element(unsigned long i);
	void get(unsigned long i, const char **data, size_t * szdata);
	void print(std::ostream & os);
};

#ifdef NVArrayIter
class NVArrayIter {
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

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
