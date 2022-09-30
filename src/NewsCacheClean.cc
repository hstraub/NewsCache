/* NewsCacheClean.cc 
 * -- (c) 1997-1998 by Thomas GSCHWIND <tom@infosys.tuwien.ac.at>
 * -- removes the least recently used articles/groups until the 
 * -- size of the spool directory is below SpoolSize
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
/*
 * Copyright (C) 2001-2003 Herbert Straub
 * - change from chain to table (performance)
 * - adding nice value for NewsCacheClean
 * - removing outdated .art files from resized .db files
 * - adding option -t (try_flag)
 * - adding option -p (print purge table) - debug option
 * 2003-03-11 Herbert Straub
 * - changing table implemention with STL vector
 * - removing all structures and function implementing the table 
 */
#include "config.h"

#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifdef HAVE_SETPRIORITY
#include <sys/resource.h>
#endif

#include <string>
#include <vector>
#include <algorithm>

#include "Debug.h"
#include "setugid.h"
#include "Config.h"
#include "Logger.h"
#include "NServer.h"

using namespace std;

#ifndef WITH_UNIQUE_PACKAGE_NAME
#undef PACKAGE
#define PACKAGE PACKAGE_NEWSCACHECLEAN
#endif

#define MAX(a,b) (((a)>(b))?(a):(b))
#define FILE_ACCESS_TABLE_ENTRIES 100000

Logger slog;
const char *cmnd;
Config Cfg;

class Entry {
public:
	Entry (const char *path, time_t time, long blocks) 
		: Path(path), atime(time), blocks(blocks) {};

	void print (ostream &out) {
		out << Path << " " << atime << " " << blocks << " " << endl;
	};
	
	string Path;
	time_t atime;
	long blocks;
};


vector<Entry*> pvector;
bool sort_entries (const Entry *l, const Entry *r);
void clean(const char *cpath);

struct cache_statistic {
	size_t stPurgedDbCnt;	/* purged .db file count */
	size_t stPurgedDirCnt;	/* purged directory count */
	size_t stPurgedArtCnt;	/* purged .art* file count */
	size_t stPurgedOutArtCnt;
				/* purged outdated .art* file count */
	size_t stDbCnt;		/* overall .db file count before purge */
	size_t stDirCnt;	/* overall directory count before purge */
	size_t stArtCnt;	/* overall .art file count before purge */
	size_t stOutArtCnt;	/* overall outdated .art file count before p.*/
	size_t stPurgedDbSize;	/* purged .db filesize */
	size_t stPurgedArtSize;	/* purged .art* filesize */
	size_t stPurgedOutArtSize;
				/* purged outdated .art* filesize */
	size_t stDbSize;	/* overall .db filesize before purge */
	size_t stArtSize;	/* overall .art* filesize before purge */
	size_t stOutArtSize;	/* overall outdated .art filesize before p. */
};

long _blocks = 0;

static void print_statistic(ostream &out, struct cache_statistic *pS);
void remove_elements(long max_blocks, struct cache_statistic *pS);

static void print_statistic(ostream &out, struct cache_statistic *pS)
{
	out << "PurgedDbCnt: " << pS->stPurgedDbCnt << endl;
	out << "PurgedDirCnt: " << pS->stPurgedDirCnt << endl;
	out << "PurgedArtCnt: " << pS->stPurgedArtCnt << endl;
	out << "PurgedOutArtCnt: " << pS->stPurgedOutArtCnt << endl;
	out << "DbCnt: " << pS->stDbCnt << endl;
	out << "DirCnt: " << pS->stDirCnt << endl;
	out << "ArtCnt: " << pS->stArtCnt << endl;
	out << "OutArtCnt: " << pS->stOutArtCnt << endl;
	out << "PurgedDbSize: " << pS->stPurgedDbSize / 2 /
	    1024 << "Mb" << endl;
	out << "PurgedArtSize: " << pS->stPurgedArtSize / 2 /
	    1024 << "Mb" << endl;
	out << "DbSize: " << pS->stDbSize / 2 / 1024 << "Mb" << endl;
	out << "ArtSize: " << pS->stArtSize / 2 / 1024 << "Mb" << endl;
	out << "OutArtSize: " << pS->stOutArtSize /2 / 1024 << "MB" << endl;
}

bool sort_entries (const Entry *l, const Entry *r)
{
	if (l->atime < r->atime)
		return 1;
	else
		return 0;
}


void remove_elements(long max_blocks, struct cache_statistic *pS)
{
	int isDb;
	char cBuffer[MAXPATHLEN];
	vector<Entry *>::iterator begin, end;

	for (begin=pvector.begin(), end=pvector.end();
	     begin != end; begin++) {
		(*begin)->Path.length();
		if (strcmp((*begin)->Path.c_str() + (*begin)->Path.length() - 3, ".db") == 0) {
			isDb = 1;
			pS->stDbCnt++;
			pS->stDbSize += (*begin)->blocks;
		} else {
			isDb = 0;
			if ((*begin)->atime != 0) {
				pS->stArtCnt++;
				pS->stArtSize += (*begin)->blocks;
			} else {
				pS->stOutArtCnt++;
				pS->stOutArtSize += (*begin)->blocks;
			}
		}
		if ((*begin)->atime != 0 && _blocks <= max_blocks)
			continue;
		if (unlink((*begin)->Path.c_str()) == 0) {
			_blocks -= (*begin)->blocks;
			if (!isDb) {
				if ((*begin)->atime != 0) {
					pS->stPurgedArtCnt++;
					pS->stPurgedArtSize += (*begin)->blocks;
				} else {
					pS->stPurgedOutArtCnt++;
					pS->stPurgedOutArtSize += (*begin)->blocks;
				}
			} else {
				pS->stPurgedDbCnt++;
				pS->stPurgedDbSize += (*begin)->blocks;
				strcpy(cBuffer, (*begin)->Path.c_str());
				cBuffer[strlen(cBuffer) - 3] = '\0';
				if (rmdir(cBuffer) == 0) {
					pS->stPurgedDirCnt++;
				} else if (errno == ENOTEMPTY) {
					slog.p(Logger::Debug) 
						<< "Error rmdir "
						<< "(Directory not empty) "
						<< (*begin)->Path << "\n";
				} else {
					slog.p(Logger::Error) 
						<< "Error rmdir "
						<< (*begin)->Path << "\n";
				}
			}
		} else {
			slog.
			    p(Logger::
			      Error) << "Error unlink " << (*begin)->Path << "\n";
		}
	}
}

void clean(const char *cpath)
{
	VERB(slog.p(Logger::Debug) << "clean(" << cpath << ")\n");
	DIR *d;
	time_t dbatime;
	char buf[MAXPATHLEN];

	struct dirent *f;
	struct stat s;

	sprintf(buf, "%s/.db", cpath);
	// Exception Directories
	if (strcmp(buf, "./.artSpool") == 0)
		return;
	if (strcmp(buf, "./.badArticles") == 0)
		return;
	if (stat(buf, &s) == 0) {
		dbatime = MAX(s.st_atime, s.st_mtime);
		pvector.push_back( new Entry (buf, dbatime, s.st_blocks) );
		_blocks += s.st_blocks;
		--dbatime;
	} else {
		dbatime = 0;
	}

	if ((d = opendir(cpath)) == NULL) {
		slog.p(Logger::Error) << "cannot open " << cpath << "\n";
		return;
	}
	while ((f = readdir(d)) != NULL) {
		if (strcmp(f->d_name, ".") == 0)
			continue;
		if (strcmp(f->d_name, "..") == 0)
			continue;
		sprintf(buf, "%s/%s", cpath, f->d_name);
		if (stat(buf, &s) == 0) {
			if (S_ISDIR(s.st_mode))
				clean(buf);
		} else {
			slog.
			    p(Logger::
			      Error) << "cannot stat " << buf << "\n";
		}
	}
	closedir(d);

	if ((d = opendir(cpath)) == NULL) {
		slog.p(Logger::Error) << "cannot reopen " << cpath << "\n";
		return;
	}
	// sanity check
	string ngname (cpath);
	// ./be/comp/os/unix -> be.comp.os.unix
	ngname.replace (0, 2, "");
	int i;
	while ((i=ngname.find ("/")) != -1) {
		ngname[i] = '.';
	}
	NVNewsgroup *ng;
	OverviewFmt *fmt = new OverviewFmt ();
	int firstnr;
	int lastnr;
	try {
		string dbname = cpath;
		dbname += "/.db";
		struct stat s;
		if (stat (dbname.c_str(), &s) == 0) {
			ng = new NVNewsgroup (fmt,
				(const char *) Cfg.SpoolDirectory,
				ngname.c_str());
		} else {
			ng = NULL;
		}
	} catch (const NoSuchGroupError &e) {
		ng = NULL;
	}
	// FIXME: what we are doing with bad database files?
	NC_CATCH_ALL ("Error, please report bug!");
	if (ng != NULL) {
		firstnr = ng->firstnbr ();
		lastnr = ng->lastnbr ();
		delete ng;
	}
	string art;
	int nr;
	while ((f = readdir(d)) != NULL) {
		sprintf(buf, "%s/%s", cpath, f->d_name);
		if (stat(buf, &s) == 0) {
			if (S_ISREG(s.st_mode) && dbatime != 0 &&
			    strncmp(f->d_name, ".art", 4) == 0) {
				time_t at = MAX(s.st_atime, s.st_mtime);
				if (dbatime <= at)
					at = dbatime;
				art = f->d_name;
				art.replace (0,4, "");
				if (sscanf (art.c_str(), "%d", &nr) != 1) {
					slog.p(Logger::Error) << "error "
						<< "parsing " << f->d_name;
				} else if ( ng == NULL | nr < firstnr || nr > lastnr) {
					at = 0;
				}
				pvector.push_back (new Entry(buf, at, 
					s.st_blocks) );
				_blocks += s.st_blocks;
			}
		} else {
			slog.
			    p(Logger::
			      Error) << "cannot stat " << buf << "\n";
		}
	}
	closedir(d);
	return;
}



#define USAGE \
"       -h, --help\n"\
"              Show summary of options.\n"\
"\n"\
"       -v, --version\n"\
"              Show version of program.\n"\
"\n"\
"       -c --configuration config-file\n"\
"\n"\
"       -s --statistic\n"\
"              Show statistic informations.\n"\
"\n"\
"       -p --print-purgetable\n"\
"              Print the sorted purge table.\n"\
"\n"\
"       -t --try\n"\
"              Try, not really removing files.\n"\


int main(int argc, char **argv)
{
#ifndef WITH_SYSLOG
	char logfile[MAXPATHLEN];
	time_t t;
	pid_t p;
#endif
	char conffile[MAXPATHLEN];
	int opt_config = 0;
	int c, aerr = 0;
	struct cache_statistic Stat;
	int nice;
	int stat=0;
	int try_flag = 0;
	char p_flag=0;
	vector<Entry *>::iterator begin, end;

	memset ((void *) &Stat, 0, sizeof (struct cache_statistic));

	sprintf(conffile, "%s/newscache.conf", SYSCONFDIR);

	cmnd = argv[0];
	while (1) {
#ifdef HAVE_GETOPT_H
		int option_index = 0;
		static struct option long_options[] = {
			{"version", 0, 0, 'v'},
			{"help", 0, 0, 'h'},
			{"configuration", 1, 0, 'c'},
			{"statistic", 0, 0, 's'},
			{"try", 0, 0, 't'},
			{"print-purgetable", 0, 0, 'p'},
			{0, 0, 0, 0}
		};

		c = getopt_long (argc, argv, "vhc:stp", long_options,
				&option_index);
#else
		c = getopt (argc, argv, "vhc:stp");
#endif

		if (c == -1)
			break;

		switch (c) {
			case 'v':
				cout << PACKAGE << " " << VERSION << endl;
				exit (0);
				break;

			case 'h':
				cout << "Usage: " << cmnd << endl;
				cout << USAGE << endl;
				exit (0);
				break;

			case 'c':
				strcpy (conffile, optarg);
				++opt_config;
				break;

			case 's':
				stat = 1;
				break;

			case 't':
				try_flag = 1;
				break;
			
			case 'p':
				p_flag = 1;
				break;

			default:
				aerr = 1;
				break;
		}
	}

	if (aerr || optind != argc) {
		cerr << "Usage: " << cmnd << " [options]\n" << USAGE;
		exit(1);
	}

	try {
		Cfg.read(conffile);
//    strcpy(nntp_hostname,Cfg.Hostname);
	}
	catch(const IOError & io) {
		cerr << "unexpected EOF in " << conffile << "\n";
		exit(2);
	}
	catch(const SyntaxError & se) {
		cerr << se._errtext << "\n";
		exit(2);
	}

#ifdef WITH_SYSLOG
	slog.open(PACKAGE, LOG_NDELAY | LOG_PID, LOG_NEWS);
#else
	sprintf (logfile, "%s/newscache_newscacheclean.log", Cfg.LogDirectory);
	slog.open (logfile);
#endif
	slog.p(Logger::Info) << "cleaning spool directory\n";


	/* set nice value for master server */
#ifdef HAVE_SETPRIORITY
	errno = 0;
	nice = Cfg.NiceServer;
	nice += getpriority(PRIO_PROCESS, 0);
	if (nice == -1 && errno != 0) {
		slog.
		    p(Logger::
		      Error) << "getpriority failed: " << strerror(errno)
		    << "\n";
	} else {
		if (setpriority(PRIO_PROCESS, 0, nice) == -1)
			slog.
			    p(Logger::
			      Error) << "setpriority failed: " <<
			    strerror(errno) << "\n";
	}
#endif

	if (chdir(Cfg.SpoolDirectory) < 0) {
		slog.
		    p(Logger::
		      Error) <<
		    "cannot change to spool directory -- exiting\n";
		exit(1);
	}
	if (getuid() == CONF_UIDROOT)
		setugid(Cfg.Username, Cfg.Groupname);
	clean(".");
	sort (pvector.begin(), pvector.end(), sort_entries);
	if (p_flag) {
		cout << "---------------- Purge table ----------------" << endl;
		for (begin=pvector.begin(), end=pvector.end();
		     begin != end; begin++) {
			(*begin)->print (cout);
		}
		cout << "---------------- Purge table ----------------" << endl;
	}
	if (!try_flag) {
		remove_elements(Cfg.SpoolSize * 2, &Stat);
	}
	if (stat) {
		print_statistic(cout, &Stat);
	}

	slog.p(Logger::Info) << "|SpoolDirectory|="
	    << (unsigned int) (_blocks / 2) << "K\n";
	return 0;
}
