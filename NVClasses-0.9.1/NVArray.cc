#include "NVArray.h"
#include "Error.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

using namespace std;

void NVArray::make_current()
{
	nvoff_t adata;

	if (!mem_p || !is_current())
		NVcontainer::make_current();
	if ((adata = getdata()) == 0) {
		arrtab = NULL;
		arrfst = 1;
		arrlst = 0;
	} else {
		arrtab =
		    (nvoff_t *) (mem_p + adata +
				 2 * sizeof(unsigned long));
		arrfst = *(unsigned long *) (mem_p + adata);
		arrlst = *((unsigned long *) (mem_p + adata) + 1);
	}
}

void NVArray::sclear(void)
{
	unsigned long i, n;

	if (arrfst > arrlst)
		return;
	n = arrlst - arrfst + 1;
	for (i = 0; i < n; i++)
		if (arrtab[i])
			nvfree(arrtab[i]);
}

void NVArray::sset(unsigned long i, const char *data, size_t szdata)
{
	nvoff_t x;

	if (arrtab[i])
		nvfree(arrtab[i]);
	x = nvalloc(szdata + sizeof(unsigned long));
	arrtab[i] = x;
	*((unsigned long *) (mem_p + x)) = szdata;
	memcpy(mem_p + x + sizeof(unsigned long), data, szdata);
}

void NVArray::sdel(unsigned long i)
{
	if (arrtab[i]) {
		nvfree(arrtab[i]);
		arrtab[i] = 0;
	}
}

int NVArray::sis_empty(void)
{
	unsigned long i, n;

	if (arrfst > arrlst)
		return 1;
	n = arrlst - arrfst + 1;
	for (i = 0; i < n; i++)
		if (arrtab[i])
			return 0;
	return 1;
}

int NVArray::shas_element(unsigned long i)
{
	if (i < arrfst || i > arrlst)
		return 0;
	return arrtab[i - arrfst];
}

void NVArray::sget(unsigned long i, char **data, size_t * szdata)
{
	if (arrfst <= i && i <= arrlst && arrtab[i = i - arrfst]) {
		*szdata = *(unsigned long *) (mem_p + arrtab[i]);
		*data = mem_p + arrtab[i] + sizeof(unsigned long);
	} else {
		*szdata = 0;
		*data = NULL;
	}
}

void NVArray::sprint(ostream & os)
{
	unsigned long i, n;

	if (arrfst > arrlst)
		return;
	n = arrlst - arrfst + 1;
	for (i = 0; i < n; i++) {
		if (arrtab[i]) {
			os << arrfst +
			    i << "/" << *(unsigned long *) (mem_p +
							    arrtab[i])
			    << ":" << mem_p + arrtab[i] +
			    sizeof(unsigned long) << endl;
		} else {
			os << arrfst + i << "/(null):" << endl;
		}
	}
}

NVArray::NVArray(const char *dbname, int flags)
	: NVcontainer()
{
	open(dbname, flags);
}

ASSERT(
	nvoff_t NVArray::nvalloc(size_t rsz)
	{
       nvoff_t r = NVcontainer::nvalloc(rsz);
       if (!arrtab) return r;
       if ((const char *) (arrtab + (arrlst - arrfst)) <=
	   mem_p + r) return r;
       if (mem_p + (r + rsz) <= (const char *) arrtab) return r;
       VERB(char buf[256];
	    sprintf(buf, "allocated illegal memory block [%lu,%lu[\n", r,
		    r + rsz); slog.p(Logger::Error) << buf);
       kill(getpid(), SIGABRT); exit(100);}

       void NVArray::nvfree(nvoff_t p) {
       if (!arrtab) {
       NVcontainer::nvfree(p); return;}
       if ((const char *) (arrtab + (arrlst - arrfst)) <= mem_p + p) {
       NVcontainer::nvfree(p); return;}
       int psz = *(unsigned long *) (mem_p + (p - sizeof(unsigned long)));
       if (mem_p + (p + psz) <= (const char *) arrtab) {
       NVcontainer::nvfree(p); return;}
       kill(getpid(), SIGABRT);
    }
)

void NVArray::open(const char *dbname, int flags)
{
	NVcontainer::open(dbname, flags);
}

void NVArray::ssetsize(unsigned long fst, unsigned long lst)
{
	nvoff_t *ndata;
	nvoff_t p, q;
	unsigned long i, n;

	p = nvalloc((lst - fst + 1 + 2) * sizeof(unsigned long));
	q = getdata();
	ndata = (nvoff_t *) (mem_p + p);
	ndata[0] = fst;
	ndata[1] = lst;
	n = lst - fst + 1;
	if (arrfst <= arrlst) {
		for (i = 0; i < n; i++) {
			if (arrfst <= i + fst && i + fst <= arrlst) {
				ndata[2 + i] = arrtab[i + fst - arrfst];
			} else {
				ndata[2 + i] = 0;
			}
		}
	} else {
		for (i = 0; i < n; i++)
			ndata[2 + i] = 0;
	}
	arrfst = fst;
	arrlst = lst;
	setdata(p);
	arrtab = ndata + 2;
	if (q)
		nvfree(q);
}

int NVArray::setsize(unsigned long fst, unsigned long lst, int flags)
{
	// Usually, the user should have already set an excludive lock
	// Otherwise, it cannot be guaranteed that no other process
	// changes the arraysize again
	// This is just to ensure that the NVArray cannot get inconsistent
	// even in the case where the user uses this database incorrectly.
	lock(ExclLock);
	int ok = 1;
	unsigned long i, n;

	if (arrfst <= arrlst) {
		// Clean up existing array or return an error
		if (fst > arrfst) {
			n = fst - arrfst + 1;
			for (i = 0; i < n; i++) {
				if (arrtab[i]) {
					if (flags & force)
						nvfree(arrtab[i]);
					else
						ok = 0;
				}
			}
		}
		if (lst < arrlst) {
			n = arrlst - arrfst + 1;
			for (i = lst - arrfst + 1; i < n; i++) {
				if (arrtab[i]) {
					if (flags & force) {
						nvfree(arrtab[i]);
						arrtab[i] = 0;
					} else
						ok = 0;
				}
			}
		}
	}
	if (ok)
		ssetsize(fst, lst);
	lock(UnLock);
	if (ok)
		return 0;
	else
		return -1;
}

void NVArray::clear(void)
{
	lock(ExclLock);
	sclear();
	lock(UnLock);
}

void NVArray::set(unsigned long i, const char *data, size_t szdata)
{
	lock(ExclLock);
	sset(i, data, szdata);
	lock(UnLock);
}

void NVArray::del(unsigned long i)
{
	lock(ExclLock);
	sdel(i);
	lock(UnLock);
}

int NVArray::is_empty(void)
{
	int r;
	lock(ShrdLock);
	r = sis_empty();
	lock(UnLock);
	return r;
}

int NVArray::has_element(unsigned long i)
{
	int r;
	lock(ShrdLock);
	r = shas_element(i);
	lock(UnLock);
	return r;
}

void NVArray::get(unsigned long i, const char **data, size_t * szdata)
{
	lock(ShrdLock);
	sget(i, (char **) data, szdata);
	lock(UnLock);
}

void NVArray::print(ostream & os)
{
	lock(ShrdLock);
	sprint(os);
	lock(UnLock);
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
