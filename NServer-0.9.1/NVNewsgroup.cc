#include<ctype.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<stdio.h>
#include<utime.h>
#include<unistd.h>

#include"Logger.h"
#include"Error.h"
#include"NVNewsgroup.h"

using namespace std;

/* Structure of a record of the NVArray
 * 4 Bytes ... Size of record (s)
 * s Bytes ... The record itself
 *
 * Structure of a record
 * 1 Byte ... Type of information (overview,article,bigarticle)
 * n Bytes ... Article/Overviewrecord, including trailing \0
 */

#ifdef ENABLE_ASSERTIONS
void NVNewsgroup::testdb(void)
{
	//  VERB(slog.p(Logger::Debug) << "NVNewsgroup::testdb()\n"); 
	char *data;
	size_t szdata;
	unsigned long f, i, l;
//   Article *art=NULL;
	int corr = 0;

	NVArray::lock(NVcontainer::ShrdLock);
	NVArray::getsize(&f, &l);
	for (i = f; i <= l; i++) {
		NVArray::sget(i, &data, &szdata);
		if (data && *data != overview && *data != article
		    && *data != bigarticle) {
			corr++;
		}
	}
	if (corr) {
		slog.
		    p(Logger::
		      Alert) << "NVNewsgroup::testdb: Oops, " << corr <<
		    " errors found in database!\n";
	}
	NVArray::lock(NVcontainer::UnLock);
}
#endif

void NVNewsgroup::sprintover(ostream & os, unsigned int nbr)
{
	char *data;
	size_t szdata;
	unsigned long f, l;
	string over;

	NVArray::getsize(&f, &l);
	if (f <= nbr && nbr <= l) {
		NVArray::sget(nbr, &data, &szdata);
		if (data) {
			if (*data == article) {
				Article *art;
				//FIX! This is inefficient!
				//FIX! The article is copied and all the overview data is copied 
				//FIX! again. We should allow Articles to use externally provided 
				//FIX! memory (eg. memory from the NVNewsgroup).
				// szdata ... Size of article+1 including the trailing \0
				art =
				    new Article(nbr, data + sizeof(char),
						szdata - sizeof(char) - 1);
				_OverviewFormat->convert(*art, over);
				os << over << "\r\n";
				delete art;
			} else if (*data == bigarticle
				   || *data == overview) {
				os << data + sizeof(char) << "\r\n";
			} else {
				// Oops, inconsistent
				VERB(slog.
				     p(Logger::
				       Alert) <<
				     "NVN::sprintover(...): inconsistent newsgroup database!\n");
			}
		}
	}
}

void NVNewsgroup::setsize(unsigned int f, unsigned int l)
{
	VERB(slog.
	     p(Logger::
	       Debug) << "NVNewsgroup::setsize(" << f << "," << l <<
	     ")\n");
	char fn[MAXPATHLEN];
	long n1, n2, i;

	lock(NVcontainer::ShrdLock);
	if (f != arrfst || l != arrlst) {
		lock(NVcontainer::ExclLock);
		if (arrfst <= arrlst) {
			// free all those elements from arrfst -> f
			i = 0;
			n1 = f - arrfst;
			n2 = arrlst - arrfst + 1;
			if (n2 < n1)
				n1 = n2;
			while ((long) i < n1) {
				if (arrtab[i]) {
					if (*
					    (mem_p + arrtab[i] +
					     sizeof(long)) == bigarticle) {
						// Remove article from disk
						sprintf(fn, "%s/.art%lu",
							_SpoolDirectory,
							arrfst + i);
						unlink(fn);
					}
					nvfree(arrtab[i]);
				}
				i++;
			}

			// free all those elements from l -> arrlst
			i = l - arrfst + 1;
			if (i < 0)
				i = 0;
			n1 = arrlst - arrfst + 1;
			while ((long) i < n1) {
				if (arrtab[i]) {
					if (*
					    (mem_p + arrtab[i] +
					     sizeof(long)) == bigarticle) {
						// Remove article from disk
						sprintf(fn, "%s/.art%lu",
							_SpoolDirectory,
							arrfst + i);
						unlink(fn);
					}
					nvfree(arrtab[i]);
				}
				i++;
			}
		}
		// resize array
		NVArray::ssetsize(f, l);
		lock(NVcontainer::UnLock);
	}
	lock(NVcontainer::UnLock);
}

Article *NVNewsgroup::getarticle(unsigned int nbr)
{
	VERB(slog.
	     p(Logger::
	       Debug) << "NVNewsgroup::getarticle(" << nbr << ")\n");
	char *data;
	size_t szdata;
	unsigned long f, l;
	Article *art = NULL;

	NVArray::lock(NVcontainer::ShrdLock);
	NVArray::getsize(&f, &l);
	if (nbr >= f && nbr <= l) {
		NVArray::sget(nbr, &data, &szdata);
		if (data) {
			if (*data == article) {
				// szdata ... Size of article including type and the trailing \0
				art =
				    new Article(nbr, data + sizeof(char),
						szdata - sizeof(char) - 1);
			} else if (*data == bigarticle) {
				char fn[MAXPATHLEN];
				ifstream fs;
				sprintf(fn, "%s/.art%u", _SpoolDirectory,
					nbr);
				fs.open(fn);
				if (!fs.good() && errno == ENOENT) {
					if ((art =
					     retrievearticle(nbr)) != NULL)
						setarticle(art);
				} else {
					art = new Article(nbr);
					art->read(fs);
				}
				fs.close();
			} else {
				// Article not available
				if ((art = retrievearticle(nbr)) != NULL)
					setarticle(art);
			}
		} else {
			// Article not available
			if ((art = retrievearticle(nbr)) != NULL)
				setarticle(art);
		}
	}
	NVArray::lock(NVcontainer::UnLock);
	return art;
}

void NVNewsgroup::setarticle(Article * art)
{
	unsigned long f, l;
	unsigned int nbr;

	VERB(char buf[1024];
	     sprintf(buf, "NVNewsgroup::setarticle(*art(nbr=%d))\n",
		     art->getnbr()); slog.p(Logger::Debug) << buf);
	ASSERT2(testdb());

	nbr = art->getnbr();
	NVArray::lock(NVcontainer::ExclLock);
	NVArray::getsize(&f, &l);
	if (f <= nbr && nbr <= l) {
		char *data;
		size_t szdata;
		unsigned long i;

		i = nbr - arrfst;
		NVArray::sget(nbr, &data, &szdata);
		if (art->length() <= 15360) {
			// Article smaller/equal 15k => Store article in NewsgroupDB
			nvoff_t x;
			if (data) {
				nvfree(arrtab[i]);
			}
			szdata = art->length() + 1 + sizeof(char);
			x = nvalloc(szdata + sizeof(unsigned long));
			arrtab[i] = x;
			data = mem_p + x;
			*((unsigned long *) data) = szdata;
			data += sizeof(unsigned long);
			*data = article;
//       VERB(sprintf(buf,"NVN::setarticle: Before memcpy arrtab=%p data=%p\n",arrtab,data);
//         slog.p(Logger::Debug) << buf);
			ASSERT2(testdb());
			memcpy(data + sizeof(char), art->c_str(),
			       art->length() + 1);
		} else if (art->length() > 15360) {
			// Article bigger 15k => Store externally
			if (data && *data != overview
			    && *data != bigarticle) {
				nvfree(arrtab[i]);
				data = NULL;
			}
			if (!data) {
				string over;
				nvoff_t x;
				_OverviewFormat->convert(*art, over);
				szdata = over.length() + 1 + sizeof(char);
				x = nvalloc(szdata +
					    sizeof(unsigned long));
				arrtab[i] = x;
				data = mem_p + x;
				*((unsigned long *) (data)) = szdata;
				data += sizeof(unsigned long);
				memcpy(data + sizeof(char), over.c_str(),
				       over.length() + 1);
			}
			*data = bigarticle;
			char fn[MAXPATHLEN];
			ofstream fs;
			sprintf(fn, "%s/.art%u", _SpoolDirectory, nbr);
			fs.open(fn);
			fs << *art;
			fs.close();
			*data = bigarticle;
		} else {
			VERB(slog.
			     p(Logger::
			       Debug) <<
			     "NVNewsgroup::setarticle: Tried to store an existing article\n");
		}
	}			/* if(f<=nbr<=l) */
	lock(NVcontainer::UnLock);
	ASSERT2(testdb());
}

void NVNewsgroup::printarticle(ostream & os, unsigned int nbr)
{
	char *data;
	size_t szdata;
	unsigned long f, l;

	NVArray::lock(NVcontainer::ShrdLock);
	NVArray::getsize(&f, &l);
	if (nbr >= f && nbr <= l) {
		NVArray::sget(nbr, &data, &szdata);
		if (data) {
			if (*data == article) {
				os << data + sizeof(char);
			} else if (*data == bigarticle) {
				Article art(nbr);
				char fn[MAXPATHLEN];
				ifstream fs;
				sprintf(fn, "%s/.art%u", _SpoolDirectory,
					nbr);
				fs.open(fn);
				art.read(fs);
				fs.close();
				os << art;
			} else {
				// Article not available
				Article *art;
				if ((art = retrievearticle(nbr)) != NULL)
					setarticle(art);
				os << *art;
				delete art;
			}
		}
	}
	NVArray::lock(NVcontainer::UnLock);
}

//FIX! Possibly, we should throw an exception, if the overview record
//FIX! cannot be found.
const char *NVNewsgroup::getover(unsigned int nbr)
{
	//  VERB(slog.p(Logger::Debug) << "NVNewsgroup::getover(over)\n"); 

	char *data;
	size_t szdata;
	unsigned long f, l;
	static string over;

	over = "";
	NVArray::lock(NVcontainer::ShrdLock);
	NVArray::getsize(&f, &l);
	if (f <= nbr && nbr <= l) {
		NVArray::sget(nbr, &data, &szdata);
		if (data) {
			if (*data == article) {
				Article *art;
				//FIX! This is inefficient!
				//FIX! The article is copied and all the overview data is copied 
				//FIX! again. We should allow Articles to use externally provided 
				//FIX! memory (eg. memory from the NVNewsgroup).
				// szdata ... Size of article+1 including the trailing \0
				art =
				    new Article(nbr, data + sizeof(char),
						szdata - sizeof(char) - 1);
				_OverviewFormat->convert(*art, over);
				delete art;
			} else if (*data == bigarticle
				   || *data == overview) {
				over = data + sizeof(char);
			} else {
				// Impossible
				VERB(slog.
				     p(Logger::
				       Critical) <<
				     "NVNewsgroup::getover. Oops, reached ?dead? code 936!!!\n");
			}
		}
	}
	NVArray::lock(NVcontainer::UnLock);
	return over.c_str();
}

void NVNewsgroup::setover(const string & over)
{
	//  VERB(slog.p(Logger::Notice) << "NVNewsgroup::setover(over)\n"); 

	unsigned long f, l, nbr;

	nbr = atoi((const char *) over.c_str());
	NVArray::lock(NVcontainer::ExclLock);
	NVArray::getsize(&f, &l);
	if (f <= nbr && nbr <= l) {
		char *data;
		size_t szdata;
		unsigned long i;

		i = nbr - arrfst;
		NVArray::sget(nbr, &data, &szdata);
		if (!data) {
			szdata = over.length() + 1 + sizeof(char);
			nvoff_t x =
			    nvalloc(szdata + sizeof(unsigned long));
			arrtab[i] = x;
			data = mem_p + x;
			*((unsigned long *) (data)) = szdata;
			data += sizeof(unsigned long);
			memcpy(data + sizeof(char), over.c_str(),
			       over.length() + 1);
			*data = overview;
		}
	}
	NVArray::lock(NVcontainer::UnLock);
	ASSERT2(testdb());
}

void NVNewsgroup::readoverdb(istream & is)
{
	VERB(slog.p(Logger::Debug) << "NVNewsgroup::readoverdb(&is)\n");

	string line1, line2;
	time_t now;

	lock(NVcontainer::ExclLock);
	for (;;) {
		nlreadline(is, line1, 0);
		if (line1 == "." || is.eof())
			break;
		if (_OverviewFormat->dotrans) {
			// Convert Record
			_OverviewFormat->convert(line1, line2);
			setover(line2);
		} else {
			setover(line1);
		}
	}

	time(&now);
	setmtime(now);
	lock(NVcontainer::UnLock);
}

void NVNewsgroup::printoverdb(ostream & os, unsigned int f, unsigned int l)
{
	VERB(slog.p(Logger::Debug) << "NVNewsgroup::printoverdb(os,"
	     << f << "," << l << ")\n");
	unsigned int i;
	lock(NVcontainer::ShrdLock);
	if (f < arrfst)
		f = arrfst;
	if (arrlst < l)
		l = arrlst;
	for (i = f; i <= l; i++)
		sprintover(os, i);
	lock(NVcontainer::UnLock);
}

void NVNewsgroup::printheaderdb(ostream & os,
				const char *header,
				unsigned int f, unsigned int l)
{
	char *data, xheader[512], *p;
	string fld;
	const char *q;
	size_t szdata;
	unsigned long gf, i, gl;

	p = xheader;
	q = header;
	while ((*p++ = *q++));
	*(p - 1) = ':';
	*p = '\0';
	NVArray::lock(NVcontainer::ShrdLock);
	NVArray::getsize(&gf, &gl);
	if (f < gf)
		f = gf;
	if (l > gl)
		l = gl;
	for (i = f; i <= l; i++) {
		NVArray::sget(i, &data, &szdata);
		if (data) {
			if (*data == article) {
				Article *art;
				//FIX! This is inefficient!
				//FIX! The article is copied and all the overview data is copied 
				//FIX! again. We should allow Articles to use externally provided 
				//FIX! memory (eg. memory from the NVNewsgroup).
				// szdata ... Size of article including the trailing \0 plus
				//            1char (= InformationTag of NVNewsgroup)
				art =
				    new Article(i, data + sizeof(char),
						szdata - sizeof(char));
				try {
					fld = art->getfield(xheader, 0);
					os << i << " " << fld << "\r\n";
				} catch(NoSuchFieldError & nfe) {
					os << i << " (none)\r\n";
				}
				delete art;
			} else if (*data == bigarticle
				   || *data == overview) {
				const char *over = data + sizeof(char);
				try {
					fld =
					    _OverviewFormat->getfield(over,
								      xheader,
								      0);
					os << i << " " << fld << "\r\n";
				} catch(NoSuchFieldError & nfe) {
					os << i << " (none)\r\n";
				}
			}
		}		/* if(data) */
	}
	NVArray::lock(NVcontainer::UnLock);
}

void NVNewsgroup::printlistgroup(ostream & os)
{
	VERB(slog.
	     p(Logger::Debug) << "NVNewsgroup::printlistgroup(&os)\n");

	unsigned int i;
	lock(NVcontainer::ShrdLock);
	for (i = arrfst; i <= arrlst; i++) {
		if (shas_element(i))
			os << i << "\r\n";
	}
	lock(NVcontainer::UnLock);
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
