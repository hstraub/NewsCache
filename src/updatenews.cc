/* updatenews.cc (c) 1996 by Thomas GSCHWIND 
 * -- prefetch a set of newsgroups
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
#include "config.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <iostream>
#include <vector>

#include "Debug.h"
#include "Config.h"
#include "NServer.h"
#include "Logger.h"
#include "setugid.h"

using namespace std;

#ifndef WITH_UNIQUE_PACKAGE_NAME
#undef PACKAGE
#define PACKAGE PACKAGE_UPDATENEWS
#endif

//Notes {
// Config überarbeiten. Bei Syntaxfehler beenden. 
// Switch ?-d? der nur Config einliest und ausgibt.
//} Notes

enum {
	F_PREFETCH_ACTIVE = 0x01,
	F_PREFETCH_GROUP = 0x02,
	F_PREFETCH_OVER = 0x04,
	F_PREFETCH_LOCKGROUP = 0x08
};

Logger slog;
const char *cmnd;
Config Cfg;
int opt_flags = 0;
int opt_workers = 0;
int Xsignal;

void catchsignal(int num)
{
	Xsignal = num;
#ifdef HAVE_SIGACTION
	/* Reinstall the signal handler */
	struct sigaction action;
	action.sa_handler = catchsignal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(num, &action, NULL);
#else
	signal(num, catchsignal);
#endif
}

void doprefetch(CServer * cs, const char *group)
{
	Newsgroup *grp;
	try {
		slog.p(Logger::Info) << "prefetching " << group << "\n";
		grp = cs->getgroup(group);
		if (opt_flags & F_PREFETCH_GROUP)
			grp->
			    prefetchGroup(opt_flags &
					  F_PREFETCH_LOCKGROUP);
		else if (opt_flags & F_PREFETCH_OVER)
			grp->prefetchOverview();
		cs->freegroup(grp);
	}
	catch(...) {
		slog.
		    p(Logger::
		      Error) << "prefetching " << group << " failed\n";
	}
}

int create_worker(CServer * cs, const char *group)
{
	int c;
	switch (c = fork()) {
	case -1:
		// failed
		slog.
		    p(Logger::
		      Error) << "fork failed, using this process\n";
		doprefetch(cs, group);
		return 0;
	case 0:
		// child
		doprefetch(cs, group);
		exit(0);
	default:
		// parent
		return 1;
		break;
	}
}

void readgroups(const char *fn, vector < string > &groups)
{
	ifstream ifs(fn);
	string group;

	if (!ifs.good()) {
		return;
	}

	for (; Xsignal < 0;) {
		nlreadline(ifs, group, 0);
		if (ifs.eof())
			break;
		if (group[0] == '!')
			continue;
		if (ifs.bad()) {
			slog.
			    p(Logger::Error) << "cannot read from " << Cfg.
			    PrefetchFile;
			break;
		}
		groups.push_back(group);
	}
	ifs.close();
}

void update(void)
{
	CServer cs(Cfg.SpoolDirectory, &(Cfg.srvrs));
	int wi = opt_workers ? 0 : -1;

	cs.setttl(Cfg.ttl_list, Cfg.ttl_desc);

	slog.p(Logger::Info) << "posting spooled articles\n";
	cs.postspooled();
	if (opt_flags & F_PREFETCH_ACTIVE) {
		slog.p(Logger::Debug) << "invalidate the ActiveDB (force update)\n";
		cs.invalidateActiveDB ();
	};
	slog.p(Logger::Info) << "prefetching active database\n";
	cs.active();

	if (!(opt_flags & (F_PREFETCH_GROUP | F_PREFETCH_OVER)))
		return;

	// read groups to prefetch
	vector < string > groups;
	vector < string >::iterator begin, end;
	readgroups(Cfg.PrefetchFile, groups);

	for (begin = groups.begin(), end = groups.end();
	     begin != end && Xsignal < 0; ++begin) {
		while (wi == opt_workers) {
			// maximum workers spawned, wait until a worker exits
			int cstat;
			int cid;
			cid = wait(&cstat);
			if (WIFSIGNALED(cstat)) {
				slog.
				    p(Logger::
				      Error) <<
				    "worker terminated unexpectedly\n";
				--wi;
			} else if (WIFEXITED(cstat)) {
				--wi;
			}
		}
		if (opt_workers == 0)
			doprefetch(&cs, begin->c_str());
		else
			wi += create_worker(&cs, begin->c_str());
	}
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
"       -a --active\n"\
"\n"\
"       -g --group\n"\
"              Prefetch articles.\n"\
"\n"\
"       -o --over\n"\
"              Prefetch overview database.\n"\
"\n"\
"       -l --lock-groups\n"\
"              Lock newsgroup, when prefetching.\n"\
"\n"\
"       -w --workers\n"\
"              Prefetch # newsgroups at once.\n"\

int main(int argc, char **argv)
{
#ifndef WITH_SYSLOG
	char logfile[MAXPATHLEN];
	time_t t;
	pid_t p;
#endif
	char conffile[MAXPATHLEN];
	int opt_config = 0, opt_flagsset = 0;
	int c, aerr = 0;

	sprintf(conffile, "%s/newscache.conf", SYSCONFDIR);

	cmnd = argv[0];
	while (1) {
#ifdef HAVE_GETOPT_H
		int option_index = 0;
		static struct option long_options[] = {
			{"version", 0, 0, 'v'},
			{"help", 0, 0, 'h'},
			{"active", 0, 0, 'a'},
			{"group", 0, 0, 'g'},
			{"over", 0, 0, 'o'},
			{"lock-group", 0, 0, 'l'},
			{"workers", 1, 0, 'w'},
			{"configuration", 1, 0, 'c'},
			{0, 0, 0, 0}
		};

		c = getopt_long (argc, argv, "vhc:agolw:", long_options,
				&option_index);
#else
		c = getopt (argc, argv, "vhc:agolw:");
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

			case 'a':
				opt_flags |= F_PREFETCH_ACTIVE;
				opt_flagsset = 1;
				break;

			case 'g':
				opt_flags |= F_PREFETCH_GROUP;
				opt_flagsset = 1;
				break;

			case 'o':
				opt_flags |= F_PREFETCH_OVER;
				opt_flagsset = 1;
				break;

			case 'l':
				opt_flags |= F_PREFETCH_LOCKGROUP;
				opt_flagsset = 1;
				break;

			case 'c':
				strcpy (conffile, optarg);
				++opt_config;
				break;

			default:
				aerr = 1;
				break;
		}

	}
	if (!opt_flagsset)
		opt_flags = F_PREFETCH_GROUP;

	if (aerr || optind != argc) {
		cerr << "Usage: " << cmnd << " [options]\n" << USAGE;
		exit(1);
	}

	try {
		Cfg.read(conffile);
		strcpy(nntp_hostname, Cfg.Hostname);
	}
	catch(IOError & io) {
		cerr << "unexpected EOF in " << conffile << "\n";
		exit(2);
	}
	catch(SyntaxError & se) {
		cerr << se._errtext << "\n";
		exit(2);
	}

	for (unsigned int i = 0; i < Cfg.srvrs.entries.size(); i++) {
		// Switch off offline mode
		Cfg.srvrs.entries[i].flags &=
		    ~(MPListEntry::F_OFFLINE | MPListEntry::F_SEMIOFFLINE);
		// Set timeout to 0
		Cfg.srvrs.entries[i].groupTimeout = 0;
	}

	// signal
	Xsignal = -1;
#ifdef HAVE_SIGACTION
	struct sigaction action;
	// signals indicating to terminate NewsCache
	action.sa_handler = catchsignal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGPIPE, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	sigaction(SIGALRM, &action, NULL);
	sigaction(SIGUSR1, &action, NULL);
	sigaction(SIGUSR2, &action, NULL);
#else
	signal(SIGHUP, catchsignal);
	signal(SIGPIPE, catchsignal);
	signal(SIGINT, catchsignal);
	signal(SIGPIPE, catchsignal);
	signal(SIGTERM, catchsignal);
	signal(SIGALRM, catchsignal);
	signal(SIGUSR1, catchsignal);
	signal(SIGUSR2, catchsignal);
#endif

#ifdef WITH_SYSLOG
	slog.open(PACKAGE, LOG_NDELAY | LOG_PID, LOG_NEWS);
#else
	sprintf (logfile, "%s/newscache_updatenews.log", Cfg.LogDirectory);
	slog.open (logfile);
#endif

	// Check the queue in each server-directory and send it off to the 
	// news server
	if (getuid() == CONF_UIDROOT)
		setugid(Cfg.Username, Cfg.Groupname);
	update();
	return 0;
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
