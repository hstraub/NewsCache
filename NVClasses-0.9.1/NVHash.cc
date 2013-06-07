#include"NVHash.h"
#include"Error.h"

#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>

using namespace std;

void NVHash::make_current()
{
	nvoff_t hdata;

	if (mem_p && is_current())
		return;

	NVlist::make_current();
	if ((hdata = getdata()) == 0) {
		hashtab = NULL;
		hashsz = 0;
	} else {
		hashtab =
		    (nvoff_t *) (mem_p + hdata + sizeof(unsigned long));
		hashsz = *(unsigned long *) (mem_p + hdata);
	}
}

void NVHash::sclear(void)
{
	unsigned long i;
	for (i = 0; i < hashsz; i++)
		NVlist::sclear((char *) &(hashtab[i]) - mem_p);
}

int NVHash::sis_empty(void)
{
	unsigned long i;
	for (i = 0; i < hashsz; i++)
		if (hashtab[i])
			return 0;
	return 1;
}

void NVHash::sadd(unsigned long h, const char *data, size_t szdata)
{
	NVlist::sprepend((char *) &(hashtab[h]) - mem_p, data, szdata);
}

void NVHash::sprint(ostream & os)
{
	unsigned long i;
	for (i = 0; i < hashsz; i++)
		NVlist::sprint((char *) &(hashtab[i]) - mem_p, os);
}

NVHash::NVHash(const char *dbname, unsigned long hashsz, int flags)
:NVlist()
{
	open(dbname, hashsz, flags);
}

void NVHash::open(const char *dbname, unsigned long nhashsz, int flags)
{
	NVlist::open(dbname, flags);
	if (!hashtab) {
		lock(ExclLock);
		if (!hashtab) {
			nvoff_t ht =
			    nvalloc(sizeof(unsigned long) +
				    nhashsz * sizeof(nvoff_t));
			unsigned long i;
			setdata(ht);

			hashsz = (*(unsigned long *) (mem_p + ht)) =
			    nhashsz;
			hashtab =
			    (nvoff_t *) (mem_p + ht +
					 sizeof(unsigned long));

			for (i = 0; i < hashsz; i++)
				hashtab[i] = 0;
		}
		lock(UnLock);
	}
	// The hashsz and hashtab variables will be updated at the next
	// make_current method-invocation
}

void NVHash::clear(void)
{
	lock(ExclLock);
	sclear();
	lock(UnLock);
}

int NVHash::is_empty(void)
{
	int r;
	lock(ShrdLock);
	r = sis_empty();
	lock(UnLock);
	return r;
}

void NVHash::add(unsigned long h, const char *data, size_t szdata)
{
	lock(ExclLock);
	sadd(h, data, szdata);
	lock(UnLock);
}

void NVHash::print(ostream & os)
{
	lock(ShrdLock);
	sprint(os);
	lock(UnLock);
}
