#ifndef _NVcontainer_h_
#define _NVcontainer_h_

#include <fcntl.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>

#include "Debug.h"

#define NVcontainer_LOCKSTACKSIZE 16

typedef unsigned long nvtime_t;
typedef unsigned long nvoff_t;

extern nvtime_t nvtime(nvtime_t * nvt);

/** 
 * \class NVcontainer
 * \author Thomas Gschwind
 * How should we define a uniform interface to access data
 * stored within an NVcontainer? Two kinds of accesses exist:
 * shared access ... uses NVcontainer's data copy
 * copy access ... copies the data at each request
 * The second version is easier to use, but reduces the performance.
 *
 * \bug Documentation is missing.
 */
class NVcontainer {
	struct Header {
		unsigned int hlen;	/* 32 */
		unsigned int version;	/* 32 */
		nvoff_t size;		/* 32/64 */
		nvtime_t mtime;		/* 32/64 */
		nvoff_t freelist;	/* 32/64 */
		nvoff_t bytes_free;	/* 32/64 */
		nvoff_t userdata;	/* 32/64 */
		 Header() {
			clear();
		} void clear() {
			hlen = sizeof(Header);
			version = 2;
			size = hlen;
			mtime = 0;
			freelist = userdata = 0;
			bytes_free = 0;
	}};
	struct FreeList {
		nvoff_t next;	/* 32/64 */
		nvoff_t size;	/* 32/64 */
	};
	int lck_stack[NVcontainer_LOCKSTACKSIZE];
	int lck_stackp;

	char mem_fn[MAXPATHLEN];
	int mem_fd;
	Header *mem_hdr;

	FreeList *o2fl(nvoff_t o) {
		return (FreeList *) (o ? mem_p + o : NULL);
	}
	nvoff_t fl2o(FreeList * r) {
		return r ? (char *) r - mem_p : 0;
	}

      protected:
	NVcontainer(void);
	NVcontainer(const char *dbname, int flags = 0);
	virtual ~ NVcontainer(void);

	char *mem_p;
	size_t mem_sz;

	/*
	 * The method make_current map the database file into shared memory.
	 * It does some sanety checks. If a error occours then the database
	 * file ist renamed and close with rename_to_bad_and_close. If the
	 * database filesize is changed, then it is necessary to 
	 * \throw Error and SystemError
	 */
	virtual void make_current(void);

	/*
	 * Check if the the the file size is equal the memory mapped size.
	 */
	virtual int is_current(void) {
		return mem_sz == mem_hdr->size;
	}
	virtual size_t resize(size_t nsz);

	nvoff_t getdata();
	nvoff_t getdatap();
	void setdata(nvoff_t d);

	nvoff_t nvalloc(size_t rsz);
	void nvfree(nvoff_t f);
      public:
	enum {
		UnLock = F_UNLCK,
		ShrdLock = F_RDLCK,
		ExclLock = F_WRLCK
	};
	enum {
		Block = 0x0,
		NoBlock = 0x1
	};

	int lock(int command, int block = Block);
	int get_lock(void);

	virtual void open(const char *dbname, int flags = 0);
	virtual int is_open(void);
	
	/*
	 * Close the file and reset the internal data.
	 */
	virtual void close(void);

	/*
	 * If th sanity check signals a bad state of the database file,
	 * then the rename_to_bad_close methods tries to rename the database.
	 * It append .bad to the current filename. 
	 * \author Herbert Straub
	 * \date 2004
	 */
	void rename_to_bad_and_close (void);

	void setmtime(nvtime_t tm, int force = 0);
	void getmtime(nvtime_t * tm);
};

/* We should add a general iterator class */
/*
class NVcontainterIter {
...
};
*/

#endif
