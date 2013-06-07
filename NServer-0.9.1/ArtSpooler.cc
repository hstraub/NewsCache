/* Copyright (C) 2003 Herbert Straub
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "Debug.h"
#include "Error.h"
#include "ArtSpooler.h"
#include "util.h"

#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif

using namespace std;

ArtSpooler::ArtSpooler(const string & spoolDir)
{
	VERB(slog.p(Logger::Debug) << "ArtSpooler::ArtSpooler ("
	     << spoolDir << ")\n");

	string tmp;

	this->spoolDir = spoolDir;
	if (mkpdir(spoolDir.c_str(), 0700) == -1) {
		string errtxt("Error creating spoolDirectory");
		errtxt += spoolDir;
		throw(Error(errtxt.c_str(), ERROR_LOCATION));
	}
	artSpool = spoolDir + "/.artSpool";
	if (mkpdir(artSpool.c_str(), 0700) == -1) {
		string errtxt("Error creating spoolDirectory");
		errtxt += artSpool;
		throw(Error(errtxt.c_str(), ERROR_LOCATION));
	}
	badArticles = spoolDir + "/.badArticles";
	if (mkpdir(badArticles.c_str(), 0700) == -1) {
		string errtxt("Error creating spoolDirectory");
		errtxt += artSpool;
		throw(Error(errtxt.c_str(), ERROR_LOCATION));
	}
	tmp = spoolDir + "/.resourceSpooler.lock";
	pLock = new ObjLock(tmp);
}

ArtSpooler::~ArtSpooler()
{
	VERB(slog.p(Logger::Debug) << "ArtSpooler::~ArtSpooler ()\n");

	delete pLock;
}

void ArtSpooler::spoolArt(Article & a)
{
	VERB(slog.
	     p(Logger::Debug) << "ArtSpooler::spoolArt (Article &)\n");
	int s;

	pLock->lockExBlk();
	s = storeArticle(artSpool, a);
	pLock->unlock();

	if (s != 0) {
		throw(Error("spoolArt: duplicateArt", ERROR_LOCATION));
	}

}

void ArtSpooler::storeBadArt(Article & a)
{
	VERB(slog.
	     p(Logger::Debug) << "ArtSpooler::storeBadArt (Article &)\n");
	int s;

	pLock->lockExBlk();
	s = storeArticle(badArticles, a);
	pLock->unlock();
	if (s != 0) {
		throw(Error("storeBadArt: duplicateArt", ERROR_LOCATION));
	}
}

int ArtSpooler::storeArticle(const string & path, Article & a)
{
	ofstream os;
	string fn, id;
	struct stat st;
	int s;

	id = createID();;
	fn = path + "/art_" + id;
	s = lstat(fn.c_str(), &st);
	if (s == -1) {
		os.open(fn.c_str());
		os << a;
		os.close();
		s = 0;
	} else {
		s = 1;
	}
	return (s);
}

Article *ArtSpooler::getSpooledArt(void)
{
	VERB(slog.p(Logger::Debug) << "ArtSpooler::getSpooledArt ()\n");
	string an;
	Article *pa = NULL;
	ifstream is;
	struct dirent *d;
	DIR *dir;

	pLock->lockExBlk();
	dir = opendir(artSpool.c_str());
	for (d = readdir(dir); d != NULL; d = readdir(dir)) {
		if (strncmp("art_", d->d_name, 4) == 0) {
			an = artSpool + "/" + d->d_name;
			pa = new Article();
			is.open(an.c_str());
			// FIXME: check status
			pa->read(is);
			is.close();
			unlink(an.c_str());
			// FIXME: check status
			break;
		}
	}
	closedir(dir);
	pLock->unlock();

	return pa;
}

string ArtSpooler::createID(void)
{
#ifdef HAVE_SSTREAM
	stringstream sb;
#else
	strstream sb;
#endif
	pid_t pid;
	struct timeval t;

	pid = getpid();
	gettimeofday(&t, NULL);
	sb << (int) pid << (int) t.tv_sec << (int) t.tv_usec;
	return sb.rdbuf()->str();
}

string ArtSpooler::extractID(Article & a)
{
	string ID = a.getfield("message-id");
	ID.replace(0, 3, "");
	ID.replace(ID.length() - 1, 1, "");
	return ID;
}
