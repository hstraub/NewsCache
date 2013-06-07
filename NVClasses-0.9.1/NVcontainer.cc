#include"NVcontainer.h"
#include"Error.h"

#include <iostream>
#include<fcntl.h>
#include<stdio.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<time.h>

#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif

using namespace std;

int nvflock(int fd, int cmnd, int block)
{
#ifdef USE_FLOCK
	int ncmnd;

	switch (cmnd) {
	case NVcontainer::UnLock:
		ncmnd = LOCK_UN;
		break;
	case NVcontainer::ShrdLock:
		ncmnd = LOCK_SH;
		break;
	case NVcontainer::ExclLock:
		ncmnd = LOCK_EX;
		break;
	default:
		DEBUG(cdbg << "myflockerr\n");
		return EINVAL;
	}
	if (block == NVList::NoBlock)
		ncmnd = ncmnd | LOCK_NB;

	return flock(fd, ncmnd);
#else
	int lck;
	struct flock ncmnd;
	int r;

	switch (cmnd) {
	case NVcontainer::UnLock:
		ncmnd.l_type = F_UNLCK;
		break;
	case NVcontainer::ShrdLock:
		ncmnd.l_type = F_RDLCK;
		break;
	case NVcontainer::ExclLock:
		ncmnd.l_type = F_WRLCK;
		break;
	default:
		ASSERT(cerr << "myflockerr\n");
		return EINVAL;
	}
	ncmnd.l_whence = SEEK_SET;
	ncmnd.l_start = 0;
	ncmnd.l_len = 0;
	if (block == NVcontainer::NoBlock)
		lck = F_SETLK;
	else
		lck = F_SETLKW;

	// Unlock, before applying the new lock
	if (ncmnd.l_type != F_UNLCK) {
		struct flock ulck;
		ulck.l_type = F_UNLCK;
		ulck.l_whence = SEEK_SET;
		ulck.l_start = 0;
		ulck.l_len = 0;
		if (fcntl(fd, lck, &ulck) < 0)
			return -1;
	}
	if ((r = fcntl(fd, lck, &ncmnd)) < 0) {
		if (errno == EACCES || errno == EAGAIN)
			errno = EWOULDBLOCK;
		return -1;
	}
	return 0;
#endif
}

nvtime_t nvtime(nvtime_t * nvt)
{
	time_t tm;
	nvtime_t nvtm;

	time(&tm);
	nvtm = tm;
	if (nvt)
		*nvt = nvtm;

	return nvtm;
}

NVcontainer::NVcontainer()
{
	lck_stackp = NVcontainer_LOCKSTACKSIZE;
	mem_fn[0] = '\0';
	mem_fd = -1;
	mem_p = NULL;
	mem_hdr = (Header *) mem_p;
}

NVcontainer::NVcontainer(const char *dbname, int flags)
{
	lck_stackp = NVcontainer_LOCKSTACKSIZE;
	mem_fd = -1;
	open(dbname, flags);
}

NVcontainer::~NVcontainer()
{
	close();
}

void NVcontainer::make_current()
{
	struct stat st;

	if (mem_p) {
		if (is_current())
			return;
		munmap(mem_p, mem_sz);
	}

	if (fstat(mem_fd, &st) < 0) {
		throw SystemError("NVContainer(20801): fstat failed",
				  errno, ERROR_LOCATION);
	}

	if ((nvoff_t) st.st_size <=
	    (nvoff_t) sizeof(Header) + (nvoff_t) sizeof(FreeList)) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::make_current(): info-record shrunk;"
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << " st.st_size: " << (unsigned long int) st.st_size
		   << " sizeof(Header): " << (unsigned long int) sizeof(Header);
		throw Error(sb.rdbuf()->str(), ERROR_LOCATION);
	}

	mem_sz = st.st_size;
	mem_p = (char *) mmap(NULL, mem_sz,
			      PROT_READ | PROT_WRITE, MAP_SHARED,
			      mem_fd, 0);
	if (mem_p == (char *) -1) {
		mem_p = NULL;
		throw SystemError("NVContainer::make_current(): mmap failed", errno,
				  ERROR_LOCATION);
	}
	mem_hdr = (Header *) mem_p;
	//Do some sanity checks on the database
	if (mem_hdr->hlen != sizeof(Header)) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::make_current(): wrong header size"
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << " mem_hdr->hlen: " << (unsigned long int) mem_hdr->hlen
		   << " sizeof(Header): " << (unsigned long int) sizeof(Header);
		rename_to_bad_and_close(); // close manipulate the member structure !!
		throw Error(sb.rdbuf()->str(), ERROR_LOCATION);
	}
	if (mem_hdr->version != 2) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::make_current(): wrong header version"
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << " mem_hdr->version: " << (unsigned long int) mem_hdr->version;
		rename_to_bad_and_close(); // close manipulate the member structure !!
		throw Error(sb.rdbuf()->str(), ERROR_LOCATION);
	}
	if (mem_sz != mem_hdr->size) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::make_current(): wrong database size stored "
		   << "in db-header - may be a hazard"
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << " mem_sz: " << (unsigned long int) mem_sz
		   << " mem_hdr->size: " << (unsigned long int) mem_hdr->size;
		slog.p(Logger::Critical) << sb.rdbuf()->str();
	}
}

size_t NVcontainer::resize(size_t nsz)
{
	if (nsz < mem_sz) {
		VERB(slog.
		     p(Logger::
		       Warning) <<
		     "NVContainer: reduction of database size not supported, ignored\n");
		return mem_sz;
	}

	lock(ExclLock);
	FreeList *flhead = o2fl(mem_hdr->freelist);

	nsz = (nsz + 0x10000) & (~0xffff);
	if (ftruncate(mem_fd, nsz) < 0) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::resize(): ftruncate failed  "
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << " nsz: " << (unsigned long int) nsz;
		throw SystemError(sb.rdbuf()->str(), errno, ERROR_LOCATION);
	}
	mem_hdr->bytes_free += nsz - mem_sz;
	flhead->size += nsz - mem_sz;
	mem_hdr->size = nsz;
	make_current();
	lock(UnLock);
	return mem_sz;
}

nvoff_t NVcontainer::getdata()
{
	return mem_hdr->userdata;
}

nvoff_t NVcontainer::getdatap()
{
	return ((char *) &mem_hdr->userdata - mem_p);
}

void NVcontainer::setdata(nvoff_t d)
{
	mem_hdr->userdata = d;
}

nvoff_t NVcontainer::nvalloc(size_t rsz)
{
	lock(ExclLock);
	FreeList *flhead = o2fl(mem_hdr->freelist);
	FreeList *cur, *prv, *res;

	// The allocated memory block has to meet several requirements
	// * aligned on 16 Byte boundary
	// * size has to be a multiple of 16 (simplifies the alignment)
	// * the allocated block has to hold its size
	//   the memory after the size indicator will be returned
	rsz = (rsz + sizeof(unsigned long) + 0xf) & (~0xf);
	if (flhead == NULL) {
		throw Error ("NVContainer(5946): Database file corrupted (freelist==NULL)",
		     ERROR_LOCATION);
	}
	// flhead points to the last free memory block available in the database
	prv = flhead;
	cur = o2fl(prv->next);
	// find the first suitable memory block in the database
	while (cur->size < rsz && cur != flhead) {
		prv = cur;
		cur = o2fl(cur->next);
	}

	if (cur == flhead) {
		// Special care has to be taken for the last free memory block
		// It must always be present! So we have to resize it, if its
		// memory is smaller than the requested size plus the size 
		// necessary to maintain the last block of free memory.
		if (flhead->size < rsz + sizeof(FreeList)) {
			nvoff_t prvoff = fl2o(prv);
			// Since it is very likely that the mapped region will change
			// during the resize operation, we will have to save the
			// currently used pointers
			int needsz = rsz + sizeof(FreeList) - flhead->size;
			resize(mem_sz + needsz);
			cur = flhead = o2fl(mem_hdr->freelist);
			prv = o2fl(prvoff);
			ASSERT(if (cur->size < rsz + sizeof(FreeList)) {
			       throw
			       AssertionError
			       ("NVContainer::nvalloc: resize failed",
				ERROR_LOCATION);}
			);
		}
	}
	ASSERT(if (cur->size < rsz) {
	       throw
	       AssertionError
	       ("NVContainer::nvalloc: did not find a large enough free memory block",
		ERROR_LOCATION);}
	);

	res = cur;

	if (cur->size < rsz + sizeof(FreeList)) {
		// The available space is too small to split?
		// This may only occur if cur _is_not_ the tail of the free blocks
		ASSERT(if (res == flhead) {
		       throw
		       AssertionError
		       ("NVContainer::nvalloc: Cannot split block of free memory at tail of database",
			ERROR_LOCATION);}
		);
		prv->next = cur->next;
		rsz = cur->size;
	} else {
		// Split the space into two parts
		cur = (FreeList *) ((char *) cur + rsz);
		cur->size = res->size - rsz;
		if (prv != res) {
			// list contains at least two blocks
			cur->next = res->next;
			prv->next = fl2o(cur);
			if (res == flhead) {
				mem_hdr->freelist = fl2o(cur);
			}
		} else {
			// list contains only one block pointing to itself
			// this must be the free memory block at the tail
			ASSERT(if (res != flhead) {
			       throw
			       AssertionError
			       ("NVList::nvalloc: Lost block of free memory at tail of database",
				ERROR_LOCATION);}
			);
			mem_hdr->freelist = cur->next = fl2o(cur);
		}
	}
	mem_hdr->bytes_free -= rsz;
	lock(UnLock);

	*(unsigned long *) res = rsz;
#ifdef FREELIST_ASSERT_ON
	char buf[256];
	int i = 10000;
	prv = flhead = o2fl(mem_hdr->freelist);;
	cur = o2fl(prv->next);

	VERB(sprintf
	     (buf, "nvalloc: checking freelist flhead=%p\n", flhead);
	     slog.p(Logger::Debug) << buf);
	while (cur != flhead && i) {
		VERB(sprintf
		     (buf, "nvalloc: elem(%lu)={%lu,%lu}\n",
		      (nvoff_t) ((char *) cur - mem_p), cur->next,
		      cur->size); slog.p(Logger::Debug) << buf);
		prv = cur;
		cur = o2fl(cur->next);
		ASSERT(if (prv >= cur) {
		       throw
		       AssertionError
		       ("NVContainer::nvalloc: freelist out of order!",
			ERROR_LOCATION);}
		);
		i--;
	}
	VERB(sprintf
	     (buf, "nvfree: tail(%lu)={%lu,%lu}\n",
	      (nvoff_t) ((char *) cur - mem_p), cur->next, cur->size);
	     slog.p(Logger::Debug) << buf);
	if (!i)
		VERB(slog.
		     p(Logger::
		       Debug) <<
		     "nvalloc: more than 10000 free blocks!\n");
#endif
	return (nvoff_t) ((char *) res - mem_p) + sizeof(unsigned long);
}

void NVcontainer::nvfree(nvoff_t p)
{
#ifdef FREELIST_ASSERT_ON
	VERB(char buf[256];
	     sprintf(buf, "NVcontainer::nvfree(%lu)\n", p);
	     slog.p(Logger::Debug) << buf);
#endif
	lock(ExclLock);

	FreeList *pb, *pe;
	int psz;
#ifdef FREELIST_ASSERT_ON
	FreeList *flhead;
	FreeList *cur, *prv;
	int i = 10000;
	prv = flhead = o2fl(mem_hdr->freelist);;
	cur = o2fl(prv->next);

	VERB(sprintf(buf, "NVContainer::nvfree: checking freelist flhead=%p\n", flhead);
	     slog.p(Logger::Debug) << buf);
	while (cur != flhead && i) {
		VERB(sprintf
		     (buf, "NVContainer::nvfree: elem(%lu)={%lu,%lu}\n",
		      (nvoff_t) ((char *) cur - mem_p), cur->next,
		      cur->size); slog.p(Logger::Debug) << buf);
		prv = cur;
		cur = o2fl(cur->next);
		ASSERT(if (prv >= cur) {
		       throw
		       AssertionError
		       ("NVContainer::nvfree: freelist out of order!",
			ERROR_LOCATION);}
		);
		i--;
	}
	VERB(sprintf
	     (buf, "NVContainer::nvfree: tail(%lu)={%lu,%lu}\n",
	      (nvoff_t) ((char *) cur - mem_p), cur->next, cur->size);
	     slog.p(Logger::Debug) << buf);
	if (!i)
		VERB(slog.
		     p(Logger::
		       Debug) << "NVContainer::nvfree: more than 10000 free blocks!\n");
#else
	FreeList *flhead = o2fl(mem_hdr->freelist);
	FreeList *cur = NULL, *prv = NULL;
#endif

	pb = o2fl(p - sizeof(unsigned long));
	psz = *(unsigned long *) pb;
	pe = (FreeList *) ((char *) pb + psz);

//  ASSERT(memset(pb,255,psz));
	ASSERT(if ((char *) pb < mem_p || mem_p + mem_sz < (char *) pe) {
	       char buf[1024];
	       close();
	       sprintf(buf,
		       "NVcontainer::nvfree: tried to free illegal memory block ([%p,%p])!\nNVcontainer mapped from [%p,%p]",
		       pb, pe, mem_p, mem_p + mem_sz);
	       throw AssertionError(buf, ERROR_LOCATION);});

	// flhead points to the last free memory block available in the database
	prv = flhead;
	cur = o2fl(prv->next);
	while (cur < pb && cur != flhead) {
		prv = cur;
		cur = o2fl(cur->next);
		ASSERT(if (prv >= cur) {
		       throw
		       AssertionError
		       ("NVContainer::nvfree: freelist out of order!",
			ERROR_LOCATION);}
		);
	}
	ASSERT(if (cur < pb) {
	       char buf[1024];
	       close();
	       sprintf(buf,
		       "NVcontainer::nvfree: tried to free unallocated memory block ([%p,%p])!\n Address must be smaller than flhead (%p)!\nNVcontainer mapped from[%p,%p]",
		       pb, pe, flhead, mem_p, mem_p + mem_sz);
	       throw AssertionError(buf, ERROR_LOCATION);});

	if ((char *) prv + prv->size == (char *) pb) {
#ifdef FREELIST_ASSERT_ON
		VERB(slog.
		     p(Logger::Debug) << "NVContainer::nvfree: adjacent to previous\n");
#endif
		// The freed block is adjacent to the previous free block
		prv->size += psz;
		if (pe == cur) {
#ifdef FREELIST_ASSERT_ON
			VERB(slog.
			     p(Logger::
			       Debug) <<
			     "NVContainer::nvfree: adjacent to next---cool\n");
#endif
			// The freed block is adjacent to the next free block too, cool :)
			if (cur == flhead)
				mem_hdr->freelist = fl2o(prv);
			prv->size += cur->size;
			prv->next = cur->next;
		}
	} else if (pe == cur) {
#ifdef FREELIST_ASSERT_ON
		VERB(slog.
		     p(Logger::Debug) << "NVContainer::nvfree: adjacent to next\n");
#endif
		// The freed block is adjacent to the next free block
		if (cur->next == fl2o(cur)) {
			// Only one block of free memory left
			ASSERT(if (flhead != cur) {
			       throw
			       AssertionError
			       ("NVContainer::nvfree: flhead lost trailing block of free memory",
				ERROR_LOCATION);}
			);
			mem_hdr->freelist = pb->next = fl2o(pb);
			pb->size = psz + cur->size;
		} else {
			pb->next = cur->next;
			pb->size = psz + cur->size;
			prv->next = fl2o(pb);
			if (cur == flhead)
				mem_hdr->freelist = fl2o(pb);
		}
	} else {
		VERB(slog.p(Logger::Debug) << "NVContainer::nvfree: not adjacent\n");
		// The freed block neither is adjacent to the previous nor to the 
		// next free block
		pb->next = fl2o(cur);
		pb->size = psz;
		prv->next = fl2o(pb);
	}
	mem_hdr->bytes_free += psz;
#ifdef FREELIST_ASSERT_ON
	i = 10000;
	prv = flhead = o2fl(mem_hdr->freelist);;
	cur = o2fl(prv->next);

	VERB(sprintf(buf, "NVContainer::nvfree: checking freelist flhead=%p\n", flhead);
	     slog.p(Logger::Debug) << buf);
	while (cur != flhead && i) {
		VERB(sprintf
		     (buf, "NVContainer::nvfree: elem(%lu)={%lu,%lu}\n",
		      (nvoff_t) ((char *) cur - mem_p), cur->next,
		      cur->size); slog.p(Logger::Debug) << buf);
		prv = cur;
		cur = o2fl(cur->next);
		ASSERT(if (prv >= cur) {
		       throw
		       AssertionError
		       ("NVContainer::nvfree: freelist out of order!",
			ERROR_LOCATION);}
		);
		i--;
	}
	VERB(sprintf
	     (buf, "NVContainer::nvfree: tail(%lu)={%lu,%lu}\n",
	      (nvoff_t) ((char *) cur - mem_p), cur->next, cur->size);
	     slog.p(Logger::Debug) << buf);
	if (!i)
		VERB(slog.
		     p(Logger::
		       Debug) << "NVContainer::nvfree: more than 10000 free blocks!\n");
#endif
	lock(UnLock);
}

int NVcontainer::lock(int command, int block)
{
	int res;
	int curLock = UnLock;

	if (lck_stackp < NVcontainer_LOCKSTACKSIZE) {
		curLock = lck_stack[lck_stackp];
	}
	if (command == UnLock) {
		// Unlock container 
		if (lck_stackp >= NVcontainer_LOCKSTACKSIZE) {
			throw
			    Error
			    ("NVContainer::lock(): Lock Stack underflow",
			     ERROR_LOCATION);
		}
		lck_stackp++;
		command =
		    lck_stackp <
		    NVcontainer_LOCKSTACKSIZE ? lck_stack[lck_stackp] :
		    UnLock;
	} else {
		// Lock container
		if (lck_stackp == 0) {
			throw
			    Error
			    ("NVContainer::lock(): Lock Stack overflow",
			     ERROR_LOCATION);
		}
		// Is already locked with excl => new lock must be excl
		if (curLock == ExclLock)
			command = ExclLock;
		lck_stack[--lck_stackp] = command;
	}
	// Relock if lock state changed
	if (curLock != command) {
		if ((res = nvflock(mem_fd, command, block)) < 0) {
			if (errno == EWOULDBLOCK && block == NoBlock) {
				lck_stackp++;
				return -1;
			}
			throw
			    SystemError("NVContainer::lock(): flock failed",
					errno, ERROR_LOCATION);
		}
		// Adapt to the new size if somebody changed the container's 
		// size
		if (command != UnLock)
			make_current();
	}
	return 0;
}

int NVcontainer::get_lock()
{
	return lck_stackp <
	    NVcontainer_LOCKSTACKSIZE ? lck_stack[lck_stackp] : UnLock;
}

/*
 */
void NVcontainer::open(const char *dbname, int flags)
{
	ASSERT(if (!dbname) throw
	       AssertionError
	       ("NVContainer::open() called with null-pointer\n",
		ERROR_LOCATION));

	if (strcmp(mem_fn, dbname) == 0)
		return;

	if (mem_fd >= 0)
		close();

	struct stat st;

	flags = 0;
	strcpy(mem_fn, dbname);
	if ((mem_fd =::open(mem_fn, O_RDWR | O_CREAT, 0644)) < 0) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::open(): cannot open("
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn))
		   << ",O_RDWR|O_CREAT)";
		throw SystemError(sb.rdbuf()->str(), errno, ERROR_LOCATION);
	}

	nvflock(mem_fd, ExclLock, Block);
	if (fstat(mem_fd, &st) < 0) {
#ifdef HAVE_SSTREAM
		stringstream sb;
#else
		strstream sb;
#endif
		sb << "NVConatiner::open(): fstat failed"
		   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn));
		throw SystemError(sb.rdbuf()->str(), errno, ERROR_LOCATION);
	}
	if (st.st_size == 0) {
		Header hdr;
		FreeList fl;
		hdr.freelist = hdr.hlen;
		hdr.size = 0x10000;
		fl.next = hdr.freelist;
		fl.size = hdr.bytes_free = hdr.size - hdr.hlen;

		if (ftruncate(mem_fd, hdr.size) < 0) {
			unlink(mem_fn);
#ifdef HAVE_SSTREAM
			stringstream sb;
#else
			strstream sb;
#endif
			sb << "NVConatiner::open(): ftruncate failed, unlinked"
			   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn));
			throw SystemError (sb.rdbuf()->str(), errno, ERROR_LOCATION);
			}
		if (write(mem_fd, &hdr, sizeof(Header)) != sizeof(Header)) {
			unlink(mem_fn);
#ifdef HAVE_SSTREAM
			stringstream sb;
#else
			strstream sb;
#endif
			sb << "NVConatiner::open(): write(info-record) failed, unlinked"
			   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn));
			throw SystemError (sb.rdbuf()->str(), errno, ERROR_LOCATION);
		}
		if (write(mem_fd, &fl, sizeof(FreeList)) !=
		    sizeof(FreeList)) {
			unlink(mem_fn);
#ifdef HAVE_SSTREAM
			stringstream sb;
#else
			strstream sb;
#endif
			sb << "NVConatiner::open(): write(free-record) failed unlinked"
			   << " mem_fn: " << ((mem_fn==NULL)? ("NULL") : (mem_fn));
			throw SystemError (sb.rdbuf()->str(), errno, ERROR_LOCATION);
		}
	}
	nvflock(mem_fd, UnLock, Block);

	//Sanity checks of an existing database will be done in 
	make_current();
}

int NVcontainer::is_open(void)
{
	return mem_fd >= 0;
}

void NVcontainer::close()
{
	if (mem_fd < 0)
		return;
	munmap(mem_p, mem_sz);
	::close(mem_fd);
	if (lck_stackp != NVcontainer_LOCKSTACKSIZE) {
		VERB(slog.
		     p(Logger::
		       Critical) <<
		     "NVcontainer::close: Forgot to unlock the container before closing it!\n");
		lck_stackp = NVcontainer_LOCKSTACKSIZE;
	}
	mem_fn[0] = '\0';
	mem_fd = -1;
	mem_p = NULL;
	mem_hdr = (Header *) mem_p;
}

void NVcontainer::rename_to_bad_and_close()
{
	string f_new = mem_fn;

	f_new += ".bad";
	slog.p(Logger::Error) << "NVcontainer::rename_to_bad_and_close: "
		<< "Error: bad database file: " << f_new << "\n";
	if (rename (mem_fn, f_new.c_str()) != 0) {
		slog.p (Logger::Error) << "NVcontainer::rename_to_bad_and_close: "
			<< "rename error: errno=" << errno << " errtxt: "
			<< strerror (errno) << "\n";
	}
	close ();
}
	
void NVcontainer::setmtime(nvtime_t tm, int force)
{
	lock(ExclLock);
	unsigned long ctm = mem_hdr->mtime;
	if (force || tm > ctm)
		mem_hdr->mtime = tm;
	lock(UnLock);
}

void NVcontainer::getmtime(nvtime_t * tm)
{
	lock(ShrdLock);
	(*tm) = mem_hdr->mtime;
	lock(UnLock);
}
