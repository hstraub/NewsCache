#include "CNewsgroup.h"

void CNewsgroup::listgroup(char *lstgrp, unsigned int f, unsigned int l)
{
	try {
		_RServer->listgroup(_NewsgroupName, lstgrp, f, l);
	} catch(ResponseError & rs) {
		// error, we should update nntpflag variable
		// Assume server has all articles
		slog.
		    p(Logger::
		      Error) <<
		    "news server does not support listgroup command\n";
		memset(lstgrp, 1, l - f + 1);
	} catch(Error & e) {
		memset(lstgrp, 1, l - f + 1);
	} catch(...) {
		slog.p(Logger::Error)
		    << "UNEXPECTED EXCEPTION WHILE ISSUEING LISTGROUP\n"
		    << "PLEASE REPORT TO h.straub@aon.at\n";
		memset(lstgrp, 1, l - f + 1);
	}
}

void CNewsgroup::sUpdateGroupInfo(unsigned int *infofp,
				  unsigned int *infolp)
{
	GroupInfo *info;
	unsigned int f, l, infof, infol;

	try {
		info = _RServer->groupinfo(_NewsgroupName);
		getsize(&f, &l);
		infof = info->first();
		infol = info->last();
		if (infof != f || infol != l) {
			if (info->first() < f) {
				// Oops news server's first article number is smaller than our's
				// clear database
				NVArray::clear();
			}
			// Set to the new size
			setsize(infof, infol);
		}
		*infofp = infof;
		*infolp = infol;
	}
	catch(Error & e) {
		slog.p(Logger::Error) << "groupinfo failed\n";
		getsize(infofp, infolp);
	}
	catch(...) {
		slog.
		    p(Logger::
		      Error) <<
		    "caught an unexpected exception from groupinfo\n";
		getsize(infofp, infolp);
	}
}

void CNewsgroup::sUpdateOverview(void)
{
	unsigned int f, l, i, infof, infol;
	char *lstgrp;
	char fn[MAXPATHLEN];
	time_t now;

	sUpdateGroupInfo(&infof, &infol);

	if ((lstgrp =
	     (char *) calloc((infol - infof + 1), sizeof(char))) == NULL) {
		slog.
		    p(Logger::
		      Critical) <<
		    "out of memory, cannot allocate lstgrp\n";
		return;
	}

	try {
		listgroup(lstgrp, infof, infol);

		if (_RServer->getnntpflags() & MPListEntry::F_LISTGROUP) {
			f = infof;
			while (f <= infol) {
				if ((NVArray::shas_element(f) && lstgrp[f - infof])
				    || (!NVArray::shas_element(f)
					&& !lstgrp[f - infof])) {
					f++;
					continue;
				}
				if (lstgrp[f - infof]) {
					// We do not have this article
					l = f;
					for (i = l + 1; i < l + 8 && i <= infol;
					     i++) {
						if (!NVArray::shas_element(i)
						    && lstgrp[i - infof])
							l = i;
					}
					_RServer->overviewdb(this, f, l);
					f = i;
				} else {
					i = f - infof;
					// We do have this article, but it has been deleted
					if (*(mem_p + arrtab[i] + sizeof(long)) ==
					    bigarticle) {
						// Remove article from disk
						sprintf(fn, "%s/.art%ud",
							_SpoolDirectory, i);
						unlink(fn);
					}
					nvfree(arrtab[i]);
					arrtab[i] = 0;
					f++;
				}
			}
		} else {
			f = infol;
			while ((f >= infof) && !NVArray::shas_element(f)) {
				f--;
			}

			/* do we have at least one new article? */
			if (f < infol)
			{
				_RServer->overviewdb(this, f + 1, infol);
                        }
		}

		time(&now);
		setmtime(now);
	} catch(Error & e) {
		slog.
		    p(Logger::
		      Error) << "updating the overview database failed(" <<
		    name() << "): " << e._errtext << "\n";
	} catch(...) {
		slog.p(Logger::Error)
		    <<
		    "UNEXPECTED EXCEPTION WHILE UPDATING OVERVIEW DATABASE\n"
		    << "PLEASE REPORT TO h.straub@aon.at\n";
	}
	free(lstgrp);
}

Article *CNewsgroup::retrievearticle(unsigned int nbr)
{
	Article *a;
	try {
		a = new Article;
		_RServer->article(_NewsgroupName, nbr, a);
	} catch(...) {
		return NULL;
	}
	return a;
}

void CNewsgroup::prefetchGroup(int lockgrp)
{
	VERB(slog.p(Logger::Debug) << "CNewsgroup::prefetchGroup()\n");

	time_t now;
	char *data;
	size_t szdata;
	unsigned int f, l, infof, infol;
	char *lstgrp;
	char fn[MAXPATHLEN];
	Article *art = NULL;

	NVArray::lock(NVcontainer::ExclLock);
	sUpdateGroupInfo(&infof, &infol);

	if ((lstgrp =
	     (char *) calloc((infol - infof + 1), sizeof(char))) == NULL) {
		NVArray::lock(NVcontainer::UnLock);
		slog.
		    p(Logger::
		      Critical) <<
		    "out of memory, cannot allocate lstgrp\n";
		return;
	}

	if (!lockgrp) {
		NVArray::lock(NVcontainer::UnLock);
		NVArray::lock(NVcontainer::ShrdLock);
	}
	f = infof;
	l = infol;
	try {
		listgroup(lstgrp, infof, infol);

		f = infof;
		while (f <= infol) {
			NVArray::sget(f, &data, &szdata);
			if (!data) {
				// we do not have the article
				if (!lstgrp[f - infof]) {
					// both, we and the news server do not have the article
					f++;
					continue;
				}
			} else {
				// we do have the article
				if ((*data == article
				     || *data == bigarticle)
				    && lstgrp[f - infof]) {
					// both, we and the news server do have the article
					f++;
					continue;
				}
			}
			if (!lockgrp) {
				NVArray::lock(NVcontainer::ExclLock);
				NVArray::sget(f, &data, &szdata);
			}
			if (lstgrp[f - infof]
			    && (!data || *data == overview)) {
				if ((art = retrievearticle(f)) != NULL) {
					setarticle(art);
					delete art;
				}
				if (!lockgrp)
					NVArray::lock(NVcontainer::UnLock);
				f++;
				continue;
			}
			if (!lstgrp[f - infof] && data) {
				int i = f - infof;
				// We do have this article, but it has been deleted
				if (*(mem_p + arrtab[i] + sizeof(long)) ==
				    bigarticle) {
					// Remove article from disk
					sprintf(fn, "%s/.art%ud",
						_SpoolDirectory, i);
					unlink(fn);
				}
				nvfree(arrtab[i]);
				arrtab[i] = 0;
				if (!lockgrp)
					NVArray::lock(NVcontainer::UnLock);
				f++;
				continue;
			}
			if (!lockgrp)
				NVArray::lock(NVcontainer::UnLock);
			f++;
		}
		time(&now);
		setmtime(now);
	}
	catch(Error & e) {
		slog.
		    p(Logger::
		      Error) << "updating the overview database failed\n";
	}
	catch(...) {
		slog.p(Logger::Error)
		    <<
		    "UNEXPECTED EXCEPTION WHILE UPDATING OVERVIEW DATABASE\n"
		    << "PLEASE REPORT TO h.straub@aon.at\n";
	}
	free(lstgrp);
	NVArray::lock(NVcontainer::UnLock);
}
