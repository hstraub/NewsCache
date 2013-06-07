#include"NVlist.h"
#include"Error.h"

#include<fcntl.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>

using namespace std;

void NVlist::sprepend(nvoff_t proot, const char *data, size_t szdata)
{
	Record *tail;
	Record *nr;
	nr = o2r(nvalloc(szdata + sizeof(Record)));
	tail = o2r(*(nvoff_t *) (mem_p + proot));

	nr->szdata = szdata;
	memcpy(nr->datap(), data, szdata);
	if (tail) {
		nr->next = tail->next;
		tail->next = r2o(nr);
	} else {
		*(nvoff_t *) (mem_p + proot) = nr->next = r2o(nr);
	}
}

void NVlist::sappend(nvoff_t proot, const char *data, size_t szdata)
{
	Record *tail;
	Record *nr;
	nr = o2r(nvalloc(szdata + sizeof(Record)));
	tail = o2r(*(nvoff_t *) (mem_p + proot));

	nr->szdata = szdata;
	memcpy(nr->datap(), data, szdata);
	if (tail) {
		nr->next = tail->next;
		*(nvoff_t *) (mem_p + proot) = tail->next = r2o(nr);
	} else {
		*(nvoff_t *) (mem_p + proot) = nr->next = r2o(nr);
	}
}

void NVlist::sremove(nvoff_t proot)
{
	Record *tail = o2r(*(nvoff_t *) (mem_p + proot));
	Record *nr = o2r(tail->next);

	if (tail == nr) {
		*(nvoff_t *) (mem_p + proot) = 0;
	} else {
		tail->next = nr->next;
	}
	nvfree(r2o(nr));
}

void NVlist::sclear(nvoff_t proot)
{
	while (*(nvoff_t *) (mem_p + proot))
		sremove(proot);
}

void NVlist::sprint(nvoff_t proot, ostream & os)
{
	int i = 0;
	Record *head = o2r(*(nvoff_t *) (mem_p + proot));
	Record *r = head;

	while (r) {
		r = o2r(r->next);
		os << i++ << ": " << r2o(r) << "(" << r->
		    szdata << "): " << r->datap() << endl;
		if (r == head)
			r = NULL;
	}
}
