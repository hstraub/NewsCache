#include"NVList.h"
#include"Error.h"

#include<fcntl.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>

using namespace std;

int NVList::is_empty()
{
	int r;
	lock(ShrdLock);
	r = sis_empty(getdatap());
	lock(UnLock);
	return r;
}

void NVList::prepend(const char *data, size_t szdata)
{
	lock(ExclLock);
	sprepend(getdatap(), data, szdata);
	lock(UnLock);
}

void NVList::append(const char *data, size_t szdata)
{
	lock(ExclLock);
	sappend(getdatap(), data, szdata);
	lock(UnLock);
}

void NVList::remove()
{
	if (getdata()) {
		lock(ExclLock);
		sremove(getdatap());
		lock(UnLock);
	}
}

void NVList::clear()
{
	lock(ExclLock);
	sclear(getdatap());
	lock(UnLock);
}

void NVList::print(ostream & os)
{
	lock(ShrdLock);
	sprint(getdatap(), os);
	lock(UnLock);
}
