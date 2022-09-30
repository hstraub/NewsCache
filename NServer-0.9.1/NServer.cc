/* This program is free software; you can redistribute it and/or modify
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
 * Portions Copyright (C) 2002 Herbert Straub
 * 	post error in a cascaded configuration fixed
 * Portions Copyright (C) 2002 Herbert Straub
 * 	convert ServerStram to *_pServerStream and handle
 * 	connect/disconnect with new/delete
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <iostream>
#include <fstream>
#include <string>
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#include "NServer.h"
#include "RNewsgroup.h"
#include "readline.h"

using namespace std;

#define NNTP_ISCODE(reply,code) ((reply)[0]==(code)[0] && \
                                 (reply)[1]==(code)[1] && \
                                 (reply)[2]==(code)[2])

char nntp_hostname[MAXHOSTNAMELEN];
char nntp_posting_host[MAXHOSTNAMELEN];

//********************
//***   NServer
//********************
NServer::NServer()
{
	VERB(slog.p(Logger::Debug) << "NServer::NServer()\n");
	if (nntp_hostname[0] == '\0') {
		strcpy(nntp_hostname, getfqdn());
	}
	slog.p(Logger::Debug) << "NServer::NServer() hostname set to: "
	    << nntp_hostname << "\n";
	_OverviewFormat = NULL;
	_ActiveDB = NULL;
}

NServer::~NServer()
{
	VERB(slog.p(Logger::Debug) << "NServer::~NServer()\n");
	if (_OverviewFormat)
		delete _OverviewFormat;
	if (_ActiveDB)
		delete _ActiveDB;
	_OverviewFormat = NULL;
	_ActiveDB = NULL;
}

void NServer::freegroup(Newsgroup * group)
{
	VERB(slog.p(Logger::Debug) << "NServer::freegroup(*group)\n");
	delete group;
}

//********************
//***   LServer
//********************
LServer::LServer(const char *spooldir)
{
	VERB(slog.
	     p(Logger::Debug) << "LServer::LServer(" << spooldir << ")\n");
	init(spooldir);
}

LServer::~LServer()
{
	VERB(slog.p(Logger::Debug) << "LServer::~LServer()\n");

	delete pSpool;
	pSpool = NULL;
}

void LServer::init(const char *spooldir)
{
	VERB(slog.
	     p(Logger::Debug) << "LServer::init(" << spooldir << ")\n");

	ASSERT(if
	       (strlen(spooldir) >
		sizeof(_SpoolDirectory) - MAXGROUPNAMELEN - 16) {
	       throw
	       AssertionError
	       ("name of spooldirectory too long. longest groupname+spooldirlength does not fit into maxpathlen",
		ERROR_LOCATION);}
	);
	strcpy(_SpoolDirectory, spooldir);
	pSpool = new ArtSpooler(spooldir);

	if (!_OverviewFormat)
		_OverviewFormat = new OverviewFmt();
	if (!_ActiveDB)
		_ActiveDB = NULL;
}

ActiveDB *LServer::active()
{
	return _ActiveDB;
}

GroupInfo *LServer::groupinfo(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "LServer::groupinfo(" << name << ")\n");
	static GroupInfo agroup;

	ASSERT(if (strlen(name) > 512) {
	       throw
	       AssertionError
	       ("LServer::groupinfo: name of newsgroup too long\n",
		ERROR_LOCATION);}
	);

	if (_ActiveDB->get(name, &agroup) < 0)
		throw NoSuchGroupError("no such group", ERROR_LOCATION);
	return &agroup;
}

Newsgroup *LServer::getgroup(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "LServer::getgroup(" << name << ")\n");
	if (!_ActiveDB->hasgroup(name))
		throw NoSuchGroupError("no such group", ERROR_LOCATION);
	return new NVNewsgroup(_OverviewFormat, _SpoolDirectory, name);
}

int LServer::post(Article * article)
{
//   TRACE(ctrace << "LServer::post(Article*)\n");
//   char fn[1024];
//   fstream fs;
//   int qnbr;

//   sprintf(fn,".outgoing/.qnbr");
//   fs.open(fn,ios::in|ios::out);
//   fs >> qnbr;
//   if(!fs.good()) {
//     qnbr=0;
//     fs.clear();
//   }
//   fs.seekp(0,ios::beg);
//   fs << qnbr+1 << endl;
//   if(!fs.good()) {
//     DEBUG(cdbg << "Failure on .outgoing/.qnbr\n");
//   }
//   fs.close();

//   DEBUG(cdbg << "LServer: qnbr=" << qnbr << endl << article << endl);

//   sprintf(fn,".outgoing/a%d",qnbr);
//   fs.open(fn,ios::out);
//   fs.unsetf(ios::skipws);
//   fs << article;
//   fs.close();
	return -1;
}

//********************
//***   RServer
//********************
RServer::RServer(MPList * serverlist)
{
	VERB(slog.p(Logger::Debug) << "RServer::RServer/1\n");
	init(serverlist);
}

RServer::~RServer()
{
	VERB(slog.p(Logger::Debug) << "RServer::~RServer()\n");
	setserverlist(NULL);
}

void RServer::init(MPList * serverlist)
{
	VERB(slog.p(Logger::Debug) << "RServer::init/1\n");

	_ServerList = serverlist;
	_CurrentServer = NULL;

	if (!_OverviewFormat)
		_OverviewFormat = new OverviewFmt();
	if (!_ActiveDB)
		_ActiveDB = NULL;
}

#define RSERVER_CONNECT_CHECK_CONNECTION(errstrg) \
    if(!_pServerStream->good()) { \
      if(i) { \
	i--; \
	sleep(1); \
	continue; \
      } \
      snprintf(buf, sizeof(buf), "Connection to %s:%s failed",\
	       _CurrentServer->hostname.c_str(),\
	       _CurrentServer->servicename.c_str()); \
      throw SystemError(buf,errno, ERROR_LOCATION); \
    }
void RServer::connect()
{
	VERB(slog.p(Logger::Debug) << "RServer::connect()\n");
	if (is_connected())
		return;

	const time_t now = time(NULL);
	if (now <
	    _CurrentServer->connectFailed + _CurrentServer->connectBackoff) {
		throw SystemError("RServer::connect: no connection to news server");
	}

	string grt, resp;
	char buf[1024];
	unsigned int i = _CurrentServer->retries;
	_pServerStream = new sockstream(_CurrentServer->flags & MPListEntry::F_SSL);

	for (;;) {
		slog.p(Logger::Debug) << "RServer::connect: Connecting to " <<
		  _CurrentServer->hostname << " to servicename " <<
		  _CurrentServer->servicename << "\n";

		const struct sockaddr *bindFrom =
		  (_CurrentServer->bindFrom.ss_family == AF_UNSPEC) ?
		  NULL : (const struct sockaddr *) &_CurrentServer->bindFrom;

		_pServerStream->connect(_CurrentServer->hostname.c_str(),
					_CurrentServer->servicename.c_str(),
					bindFrom,
					sizeof(_CurrentServer->bindFrom),
					_CurrentServer->connectTimeout);
		if (!_pServerStream->good()) {
			delete _pServerStream;
			_pServerStream = NULL;
			_CurrentServer->connectFailed = time(NULL);
			throw
			    SystemError
			    ("RServer::connect: cannot connect to news server",
			     errno, ERROR_LOCATION);
		}
		_pServerStream->setnodelay();
		_pServerStream->unsetf(ios::skipws);

		//FIXME! repl nlreadline by nntpreadline that transparently
		//FIXME! provides auth? use a authenticator callback class, that 
		//FIXME! supplies all the negotiation between client/server. Will
		//FIXME! get a server and a client handle as well as server and 
		//FIXME! groupname to send correct passwd to correct server/group
		//FIXME! RECONNECT/RETRY? HOW IMPLEMENT? What happens if module 
		//FIXME! cannot handle this?!?
		nlreadline(*_pServerStream, grt, 0);
		slog.p(Logger::Info) << grt << "\n";

		RSERVER_CONNECT_CHECK_CONNECTION
		    ("Connection to %s:%s failed");
		if (grt[0] != '2' || grt[1] != '0'
		    || (grt[2] != '0' && grt[2] != '1')) {
			string c("_connect_"), e("20[01]");
			delete _pServerStream;
			_pServerStream = NULL;
			_CurrentServer->connectFailed = time(NULL);
			throw ResponseError(c, e, grt);
		}

		if (_CurrentServer->nntpflags & MPListEntry::F_MODE_READER) {
			// mode reader
			resp = issue("mode reader\r\n", NULL);
			RSERVER_CONNECT_CHECK_CONNECTION
			    ("Cannot read data from %s:%s");
			if (resp[0] != '2' || resp[1] != '0'
			    || (resp[2] != '0' && resp[2] != '1')) {
				slog.
				    p(Logger::
				      Warning) <<
				    "mode reader failed, ignored\n";
				_CurrentServer->nntpflags &=
				    ~MPListEntry::F_MODE_READER;
			}
			grt = resp;
		}
		if (grt[2] == '1') {
			_CurrentServer->nntpflags &= ~MPListEntry::F_POST;
		}

		if (_CurrentServer->user[0]) {
			snprintf(buf, sizeof(buf), "authinfo user %s\r\n",
				 _CurrentServer->user.c_str());
			resp = issue(buf, NULL);
			if (resp[0] == '3') {
				snprintf(buf, sizeof(buf), "authinfo pass %s\r\n",
					 _CurrentServer->passwd.c_str());
				resp = issue(buf, NULL);
			}
			if (resp[0] != '2') {
				string c("_authenticate_"), e("[23]..");
				delete _pServerStream;
				_pServerStream = NULL;
				_CurrentServer->connectFailed = time(NULL);
				throw ResponseError(c, e, resp);
			}
		}

		if (_CurrentServer->
		    nntpflags & MPListEntry::F_LIST_OVERVIEW_FMT) {
			resp = issue("list overview.fmt\r\n", "215");
			_OverviewFormat->readxoin(*_pServerStream);
			if (resp[0] != '2' || resp[1] != '1'
			    || resp[2] != '5') {
				slog.
				    p(Logger::
				      Warning) <<
				    "list overview.fmt failed, ignored\n";
				_CurrentServer->nntpflags &=
				    ~MPListEntry::F_LIST_OVERVIEW_FMT;
				goto rserver__connect_no_list_overview_fmt;
			}
		} else {
		      rserver__connect_no_list_overview_fmt:
#ifdef HAVE_SSTREAM
			istringstream standardOverviewFmt
#else
			istrstream standardOverviewFmt
#endif
			    ("Subject:\nFrom:\nDate:\nMessage-ID:\nReferences:\nBytes:\nLines:\n");
			_OverviewFormat->readxoin(standardOverviewFmt);
		}

		if (_CurrentGroup.name()[0] != '\0') {
			selectgroup(_CurrentGroup.name(), 1);
		}
		_CurrentServer->connectFailed = 0;
		return;
	}
}

#undef RSERVER_CONNECT_CHECK_CONNECTION

void RServer::disconnect()
{
	VERB(slog.p(Logger::Debug) << "RServer::disconnect()\n");
	if (_pServerStream != NULL) {
		*_pServerStream << "quit\r\n";
		// Who is interested in this return code? 
		delete _pServerStream;
		_pServerStream = NULL;
	}
}

string RServer::issue(const char *command, const char *expresp)
{
	string req, resp;
	int rs;
	int i = _CurrentServer->retries + 1;

	req = "issue ";
	req.append(command, strlen(command) - 2);
	req.append("\n");
	slog.p(Logger::Info) << "RServer::issue: " << req.c_str();
	if (!is_connected())
		connect();
	// Send command
	for (;;) {
		try {
			*_pServerStream << command << flush;
		}
		catch(...) {
			slog.
			    p(Logger::
			      Error) <<
			    "RServer::issue: error in command sending, good: "
			    << _pServerStream->good() << "\n";
		}
		try {
			rs = nlreadline(*_pServerStream, resp, 0);
		}
		catch(...) {
			slog.
			    p(Logger::
			      Error) <<
			    "RServer::issue: error in command readline\n";
		}
		// If the remote server closed the connection (400), reconnect
		if (!_pServerStream->good()) {
			slog.
			    p(Logger::
			      Warning) <<
			    "RServer::issue: no connection to news server\n";
		} else if (strncmp(resp.c_str(), "400", 3) == 0
			   || strncmp(resp.c_str(), "503", 3) == 0) {
			slog.
			    p(Logger::
			      Warning) <<
			    "RServer::issue: server closed connection---reconnecting\n";
		}
		/*
		   else if(strncmp(resp.c_str(),"480",3)==0) {
		   slog.p(Logger::Warning) << "server requests authentication\n";
		   req="authinfo user ";
		   req.append(_CurrentServer->Username);
		   slog.p(Logger::Debug) << req.c_str();
		   req.append("\r\n");
		   _ServerStream << req;
		   rs=nlreadline(_ServerStream,resp,0);
		   } */
		else {
			break;
		}

		// If we have lost the connection to the news server, we try
		// to reconnect to the server and reissue the command.
		for (;;) {
			i--;
			if (i == 0)
				throw
				    SystemError
				    ("maximum number of retries reached",
				     -1, ERROR_LOCATION);
			disconnect();
			try {
				connect();
				break;
			}
			catch(SystemError & se) {
			}
		}
	}

	// Check the response code
	if ((rs < 3)
	    || (expresp
		&& strncmp(expresp, resp.data(), strlen(expresp)))) {
		string c(command), e(expresp);
		throw ResponseError(c, e, resp);
	}

	return resp;
}


void RServer::setserver(MPListEntry * server)
{
	VERB(slog.p(Logger::Debug) << "RServer::setserver({"
	     << server->hostname << "," << server->servicename << "})\n");

	if (_CurrentServer == server)
		return;

	if (_CurrentServer)
		disconnect();
	_CurrentGroup.init();
	_CurrentServer = server;
}

void RServer::selectgroup(const char *name, int force)
{
	VERB(slog.
	     p(Logger::Debug) << "RServer::selectgroup(" << name << ")\n");
	ASSERT(if (strlen(name) > 512) {
	       throw
	       AssertionError
	       ("RServer::selectgroup: name of newsgroup too long\n",
		ERROR_LOCATION);}
	);

	if (!force && strcmp(_CurrentGroup.name(), name) == 0)
		return;

	char buf[MAXPATHLEN];
	const char *ep, *p;
	int nbr, fst, lst;
	MPListEntry *mpe;
	string resp;

	// Find the server responsible for this newsgroup
	if ((mpe = _ServerList->server(name)) == NULL) {
		throw NoSuchGroupError("no such group", ERROR_LOCATION);
	}

	setserver(mpe);
	snprintf(buf, sizeof(buf), "group %s\r\n", name);
	resp = issue(buf, NULL);

	p = resp.c_str();
	if (!NNTP_ISCODE(p, "211")) {
		if (NNTP_ISCODE(p, "411"))
			throw NoSuchGroupError("no such group",
					       ERROR_LOCATION);
		string c(buf), e("211");
		throw ResponseError(c, e, resp);
	}

	p += 3;
	nbr = strtol(p, (char **) &p, 10);
	fst = strtol(p, (char **) &p, 10);
	lst = strtol(p, (char **) &ep, 10);

	if (ep == resp) {
		//THROW!?! _Response_should_be_
		// 211 $n $f $l $g selected
		// $n Nbr. of articles in group
		// $f Nbr. of first article
		// $l Nbr. of last article
		// $g Name of newsgroup
		slog.p(Logger::Error)
		    <<
		    "response from news server not in >>211 n f l g ...<< format\n";
		// If we cannot parse the response code, use the first/last number 
		// and number of articles from the active database
		if (_ActiveDB->get(name, &_CurrentGroup) < 0) {
			// If the above call fails, _ActiveDB will be updated with a
			// probably inconsistent posting flag ('y')
			_CurrentGroup.set(name, 0, 0, 0);
			_ActiveDB->set(_CurrentGroup);
		}
	} else {
		_ActiveDB->get(name, &_CurrentGroup);
		// If the above call fails, _ActiveDB will be updated with a
		// probably inconsistent posting flag ('y')

		if (mpe->limitgroupsize) {
			if (lst - fst + 1 > mpe->limitgroupsize) {
				fst = lst - mpe->limitgroupsize + 1;
				nbr = mpe->limitgroupsize;
			}
		}

		_CurrentGroup.set(name, fst, lst, nbr);
		_ActiveDB->set(_CurrentGroup);
	}
}

void RServer::setserverlist(MPList * serverlist)
{
	VERB(slog.p(Logger::Debug) << "RServer::setserverlist()\n");
	// Disconnect
	disconnect();

	// Clean up
	_CurrentGroup.init();
	if (_ActiveDB) {
		delete _ActiveDB;
		_ActiveDB = NULL;
	}
	_CurrentServer = NULL;
	_ServerList = serverlist;
}

ActiveDB *RServer::active()
{
	VERB(slog.p(Logger::Debug) << "RServer::active()\n");
	unsigned int i, flags;
	char cgroup[MAXGROUPNAMELEN + 1], *cgp, buf[1024];
	char c;

	ASSERT(if (!_ServerList) {
	       throw
	       AssertionError
	       ("RServer::active: _ServerList is a null-pointer",
		ERROR_LOCATION);}
	);

	for (i = 0; i < _ServerList->entries.size(); i++) {
		if (!_ServerList->entries[i].hostname[0])
			continue;
		// Connect to ith news server
		try {
			setserver(&(_ServerList->entries[i]));
			if (_CurrentServer->
			    flags & (MPListEntry::F_OFFLINE | MPListEntry::
				     F_SEMIOFFLINE))
				continue;
			flags =
			    ((_CurrentServer->
			      nntpflags & MPListEntry::F_POST)
			     || !(_CurrentServer->
				  flags & MPListEntry::
				  F_SETPOSTFLAG)) ? 0 : ActiveDB::
			    F_STORE_READONLY;

			connect();
			if (_CurrentServer->
			    nntpflags & MPListEntry::
			    F_LIST_ACTIVE_WILDMAT) {
				try {
					std::string::const_iterator sp =
					  _CurrentServer->read.begin();
					const std::string::const_iterator end =
					  _CurrentServer->read.end();
					do {
						// Extract a newsgroup-expression
						cgp = cgroup;
						while (sp != end &&
						       (c = *sp++) != ',')
							*cgp++ = c;
						*cgp = '\0';
						snprintf(buf, sizeof(buf),
							 "list active %s\r\n",
							 cgroup);
						issue(buf, "215");
						// read active database and filter unwanted groups
						_ActiveDB->
						    read(*_pServerStream,
							 _ServerList->
							 makeFilter(i,
								    cgroup).c_str(),
							 flags);
					} while (sp != end);
				}
				catch(ResponseError & re) {
					slog.p(Logger::Notice)
					    <<
					    "list active [wildmat] failed for "
					    << _ServerList->entries[i].
					    hostname << ":" <<
					    _ServerList->entries[i].
					    servicename << "\n";
					_CurrentServer->nntpflags &=
					    ~MPListEntry::
					    F_LIST_ACTIVE_WILDMAT;
				}
			}
			if (!
			    (_CurrentServer->
			     nntpflags & MPListEntry::
			     F_LIST_ACTIVE_WILDMAT)) {
				issue("list active\r\n", "215");
				_ActiveDB->read(*_pServerStream,
						_ServerList->makeFilter(i,
									"*").c_str(),
						flags);
			}
		}
		catch(ResponseError & re) {
			slog.p(Logger::Error)
			    << "retrieval of activedb failed for "
			    << _ServerList->entries[i].hostname << ":"
			    << _ServerList->entries[i].servicename << "\n";
		}
		catch(SystemError & se) {
			slog.p(Logger::Warning)
			    << "connection to "
			    << _ServerList->entries[i].hostname << ":"
			    << _ServerList->entries[i].
			    servicename << " failed\n";
		}
		catch(Error & e) {
			slog.p(Logger::Alert)
			    <<
			    "UNEXPECTED EXCEPTION WHILE RETRIEVING ACTIVEDB FROM "
			    << _ServerList->entries[i].
			    hostname << ":" << _ServerList->entries[i].
			    servicename <<
			    "\nPLEASE REPORT TO h.straub@aon.at\n";
			e.print();
		}
	}
	return _ActiveDB;
}

GroupInfo *RServer::groupinfo(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "RServer::groupinfo(" << name << ")\n");
	static GroupInfo agroup;

	ASSERT(if (strlen(name) > 510) {
	       throw
	       AssertionError
	       ("RServer::groupinfo: name of newsgroup too long\n",
		ERROR_LOCATION);}
	);

	// If the newsgroup cannot be found within the active database
	// reload the database from the news server and try again.
	if (_ActiveDB->get(name, &agroup) < 0) {
		selectgroup(name, 1);
		agroup = _CurrentGroup;
	}
	return &agroup;
}

Newsgroup *RServer::getgroup(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "RServer::getgroup(" << name << ")\n");

	selectgroup(name);
	RNewsgroup *grp = new RNewsgroup(this, _OverviewFormat, name);
	grp->setsize(_CurrentGroup.first(), _CurrentGroup.last());
	return grp;
}

void RServer::listgroup(const char *gname, char *lstgrp,
			unsigned int f, unsigned int l)
{
	VERB(slog.
	     p(Logger::
	       Debug) << "RServer::listgroup(" << gname << ",...)\n");
	const char *p;
	unsigned int i;
	string resp, line;

	selectgroup(gname);
	if (_CurrentServer->nntpflags & MPListEntry::F_LISTGROUP) {
		resp = issue("listgroup\r\n", NULL);
		p = resp.c_str();
		if (!NNTP_ISCODE(p, "211")) {
			_CurrentServer->nntpflags &= ~MPListEntry::F_LISTGROUP;
			string c("listgroup\r\n"), e("211");
			throw ResponseError(c, e, resp);
		}

		for (;;) {
			nlreadline(*_pServerStream, line);
			if (line == ".\r\n")
				break;
			if (!_pServerStream->good())
				throw
					SystemError("error while reading from server",
						    errno, ERROR_LOCATION);
			i = atoi(line.data());
			if (f <= i && i <= l)
				lstgrp[i - f] = 1;
		}
	} else {
		for (i = f; i <= l; i++) {
			lstgrp[i - f] = 1;
		}
	}
}

void RServer::overviewdb(Newsgroup * ng, unsigned int fst,
			 unsigned int lst)
{
	VERB(slog.p(Logger::Debug) << "RServer::overviewdb(*ng,"
	     << fst << "-" << lst << ")\n");
	ASSERT(if (!ng) {
	       throw
	       AssertionError
	       ("RServer::overviewdb: ng parameter is a null-pointer",
		ERROR_LOCATION);}
	);

	char buf[513];
	string resp;
	const char *p;
	const char *fmt = "over %d-%d\r\n";

	selectgroup(ng->name());
	for (;;) {
		if (!(_CurrentServer->nntpflags & MPListEntry::F_OVER))
			fmt = "xover %d-%d\r\n";
		if (!(_CurrentServer->nntpflags & MPListEntry::F_XOVER)) {
			slog.
			    p(Logger::
			      Critical) <<
			    "remote news server neither implements OVER nor XOVER\n";
			throw
			    NSError
			    ("news server neither implements OVER nor XOVER",
			     ERROR_LOCATION);
		}

		snprintf(buf, sizeof(buf), fmt, fst, lst);
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (NNTP_ISCODE(p, "224")) {
			ng->readoverdb(*_pServerStream);
			return;
		}
		if (NNTP_ISCODE(p, "502")) {
			throw NotAllowedError(resp, ERROR_LOCATION);
		}
		if (NNTP_ISCODE(p, "412")) {
			slog.
			    p(Logger::
			      Warning) <<
			    "newsgroup disappeared while working on it\n";
			throw NoSuchGroupError("no such group",
					       ERROR_LOCATION);
		}
		if (NNTP_ISCODE(p, "420"))
			throw ResponseError(buf, "224", resp);
		if (fmt[0] == 'o')
			_CurrentServer->nntpflags &= ~MPListEntry::F_OVER;
		else
			_CurrentServer->nntpflags &= ~MPListEntry::F_XOVER;
	}
}

void RServer::article(const char *gname, unsigned int nbr, Article * art)
{
	VERB(slog.p(Logger::Debug) << "RServer::article(" << nbr << ")\n");
	char buf[513];
	const char *p;
	string resp;

	selectgroup(gname);

	snprintf(buf, sizeof(buf), "article %u\r\n", nbr);
	resp = issue(buf, NULL);
	p = resp.c_str();
	if (strncmp(p, "220", 3) != 0) {
		// 412 cannot happen since we have already selected the newsgroup
		// 420 -"- since we specified the article nbr
		// 430 -"- since we specified the article nbr, not the art-id
		if (strncmp(p, "423", 3) == 0)
			throw NoSuchArticleError(resp, ERROR_LOCATION);
		string c(buf), e("220");
		throw ResponseError(c, e, resp);
	}
	art->read(*_pServerStream);
	art->setnbr(nbr);
}

void RServer::article(const char *id, Article * art)
{
	VERB(slog.p(Logger::Debug) << "RServer::article(" << id << ")\n");
	char buf[1024];
	const char *p;
	string resp;

	snprintf(buf, sizeof(buf), "article %s\r\n", id);
	if (_CurrentServer) {
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (strncmp(p, "220", 3) == 0) {
			art->read(*_pServerStream);
			art->setnbr(-1);
			return;
		}
		// 412 cannot happen since we specified the article id
		// 420 -"- since we specified the article id
		// 423 -"- since we specified the article id, not the art nbr
		if (strncmp(p, "430", 3) != 0) {
			slog.p(Logger::Notice)
			    <<
			    "illegal response code to <article <id>> request\n"
			    << p;
		}
	}
	// check whether the article can be found at one of the other
	// news servers
	MPListEntry *cs = _CurrentServer;

	for (unsigned int i = 0; i < _ServerList->entries.size(); i++) {
		if (cs == &(_ServerList->entries[i]) ||
		    !_ServerList->entries[i].hostname[0])
			continue;
		setserver(&(_ServerList->entries[i]));
		connect();
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (strncmp(p, "220", 3) == 0) {
			art->read(*_pServerStream);
			art->setnbr(-1);
			return;
		}
		if (strncmp(p, "430", 3) != 0) {
			slog.p(Logger::Notice)
			    <<
			    "illegal response code to <article <id>> request\n"
			    << p;
		}
	}
	throw NoSuchArticleError(resp, ERROR_LOCATION);
}

void RServer::post(MPListEntry * srvr, Article * article)
{
	string resp;
	const char *p;
	char buf[513];

	VERB(slog.p(Logger::Debug) << "RServer::post({"
	     << srvr->hostname << "," << srvr->servicename << "}\n");

	setserver(srvr);	// clears current group selection
	connect();

	// Before posting an article, we should check whether the article
	// exists already on the upstream news server. (Since the post
	// command does not indicate duplicates, we have to check this
	// manually beforehand.

	try {
		resp = article->getfield("message-id:");
		snprintf(buf, sizeof(buf), "stat %s\r\n", resp.c_str());
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (strncmp(p, "223", 3) == 0) {
			throw DuplicateArticleError("Response 223",
						    ERROR_LOCATION);
		} else if (strncmp(p, "430", 3) != 0) {
			if (strncmp(p, "423", 3) == 0) {
				// some broken newsservers (like
				// groups.gandi.net) appear to return
				// 423 instead of 430 - log it and
				// carry on
				slog.p(Logger::Notice)
				    <<
				    "illegal response code to <stat <id>> request\n"
				    << p;
			} else {
				string c(buf), e("223|430");
				throw ResponseError(c, e, resp);
			}
		}
	}
	catch(NoSuchFieldError e) {
		VERB(slog.
		     p(Logger::
		       Debug) << "RServer::post message-id not found\n");
	}

	for (int i = _CurrentServer->retries + 1;;) {
		resp = issue("post\r\n", NULL);
		p = resp.c_str();
		if (strncmp(p, "340", 3) != 0) {
			if (strncmp(p, "440", 3) == 0)
				throw NotAllowedError(resp,
						      ERROR_LOCATION);
			throw ResponseError("post\r\n", "[34]40", resp);
		}
		*_pServerStream << *article << ".\r\n" << flush;
		nlreadline(*_pServerStream, resp);
		if (_pServerStream->good())
			break;

		// If we have lost the connection to the news server, we try
		// to reconnect to the server and reissue the command.
		slog.
		    p(Logger::
		      Warning) <<
		    "lost connection to news server unexpectedly\n";
		for (;;) {
			i--;
			if (i == 0)
				throw
				    SystemError
				    ("maximum number of retries reached while trying to post an article",
				     -1, ERROR_LOCATION);
			disconnect();
			try {
				connect();
				break;
			}
			catch(SystemError & se) {
			}
		}
	}

	p = resp.c_str();
	if (strncmp("240", p, 3) != 0) {
		if (strncmp("440", p, 3) == 0)
			throw NotAllowedError(resp, ERROR_LOCATION);
		if (strncmp("441", p, 3) == 0)
			throw PostingFailedError(resp, ERROR_LOCATION);
		throw ResponseError("post\r\n", "[23]40,44[01]", resp);
	}
}

#define TOBASE36(i,j,p) while(j) {\
	(i)=(j)%36; (j)/=36;\
	if((i)<10) *(p)++='0'+(i);\
	else *(p)++='a'+(i)-10;\
}
int RServer::post(Article * article)
{
	VERB(slog.p(Logger::Debug) << "RServer::post(Article)\n");

	// do some sanity checks
	string newsgroups;
	try {
		if (!article->has_field("from:"))
			throw InvalidArticleError("no from field",
						  ERROR_LOCATION);
		if (!article->has_field("subject:"))
			throw InvalidArticleError("no subject field",
						  ERROR_LOCATION);
		newsgroups = article->getfield("newsgroups:");
	}
	catch(NoSuchFieldError & n) {
		throw InvalidArticleError("no newsgroups field",
					  ERROR_LOCATION);
	}

	static int pc = 1;
	const char *p, *q;
	string cgroup;
	char buf[MAXHOSTNAMELEN + 256];
	char msgid[MAXHOSTNAMELEN + 256];
	MPListEntry **mpe;
	MPListEntry *c;
	int sc = 0, i;

	//FIXME! We should use the path field here, or introduce a 
	//FIXME! new field that shows the cache-posting-chain
	if (nntp_posting_host[0]) {
		snprintf(buf, sizeof(buf), "X-NNTP-Posting-Host: %s\r\n",
			 nntp_posting_host);
		article->setfield("X-NNTP-Posting-Host:", buf);
	}

	if (!(_CurrentServer->flags & MPListEntry::F_DONTGENMSGID) &&
	    !article->has_field("message-id:")) {
		struct timeval tv;
		int j;
		char mid[768];
		char *p;
		p = mid;
		gettimeofday(&tv, NULL);
		TOBASE36(i, tv.tv_sec, p);
		*p++ = '$';

		j = getpid();
		TOBASE36(i, j, p);
		*p++ = '$';

		TOBASE36(i, pc, p);
		*p++ = '@';
		strcpy(p, nntp_hostname);
		pc++;

		snprintf(msgid, sizeof(msgid),
			 "Message-ID: <newscache$%s>\r\n", mid);
		article->setfield("Message-ID:", msgid);
	}

	if ((mpe =
	     (MPListEntry **) malloc(_ServerList->entries.size() *
				     sizeof(MPListEntry *))) == NULL) {
		throw
		    SystemError("cannot allocate buffer for post servers",
				errno, ERROR_LOCATION);
	}

	q = newsgroups.c_str();
	while (isspace(*q) || *q == ',')
		q++;
	do {
		p = q;
		while (*q && *q != ',' && !isspace(*q))
			q++;
		cgroup.assign(p, q - p);
		c = _ServerList->postserver(cgroup.c_str());
		if (c) {
			i = 0;
			while (i < sc && c != mpe[i])
				i++;
			if (i == sc) {
				mpe[sc] = c;
				sc++;
			}
		} else {
			slog.p(Logger::Info)
			    << "posting to unknown newsgroup: " << cgroup
			    << "\n";
		}
		while (isspace(*q) || *q == ',')
			q++;
	} while (*q);

	if (!sc) {
		slog.p(Logger::Info) << "no news server configured for "
		    << newsgroups << "\n";
		free(mpe);
		throw InvalidArticleError("no valid newsgroup",
					  ERROR_LOCATION);
	}

	int posts = 0, spool = 0, err = 0;
	i = 0;
	// Error classification:
	// * Duplicate Articles ... Since we use our hostname and people should 
	//   be forbidden to use wrong hostnames, we assume that a duplicate 
	//   article means that the article arrived already somehow on that 
	//   server.
	// * NotAllowed ... user wants to post to a private newsgroup
	// * PostingFailed ... user submitted badly formatted article
	// * Response, System ... problem on remote site occurred
	//
	//FIX! If this library should be used for other purposes than just 
	//FIX! for NewsCache, errors should be resolved using a callback
	//FIX! class!
	do {
		try {
			if (mpe[i]->flags & MPListEntry::F_OFFLINE) {
				++spool;
			} else {
				post(mpe[i], article);
				++posts;
			}
		}
		catch(DuplicateArticleError & dae) {
			++posts;
		}
		catch(NotAllowedError & nae) {
			err |= 1;
		}
		catch(PostingFailedError & pfe) {
			err |= 2;
		}
		catch(ResponseError & re) {
			++spool;
		}
		catch(SystemError & se) {
			++spool;
		}
		catch(Error & e) {
			err |= 4;
		}
		++i;
	} while (i < sc);

	free(mpe);
	if (!err)
		return spool ? -1 : 0;
	if (err && !posts) {
		throw
		    InvalidArticleError
		    ("illegaly formatted article or no post permission",
		     ERROR_LOCATION);
	}
	// Hm, on some news servers posting succeeded on others not. 
	// What should we do now? Spool it to the error-log?
	//FIXME! Report posting as OK and send mail to posting indicating the 
	//FIXME! failure.
	return -2;
}

#undef TOBASE36

//********************
//***   CServer
//********************
CServer::CServer(const char *spooldir, MPList * serverlist)
{
	VERB(slog.
	     p(Logger::
	       Debug) << "CServer::CServer(" << spooldir <<
	     ",*serverlist)\n");
	char buf[MAXPATHLEN];

	ASSERT(if (!serverlist) {
	       slog.
	       p(Logger::
		 Notice) <<
	       "CServer::CServer: serverlist is a null-pointer\n";}
	);
	if (!_OverviewFormat) {
		_OverviewFormat = new OverviewFmt;
	}
	if (!_ActiveDB) {
		snprintf(buf, sizeof(buf), "%s/.active", spooldir);
		_ActiveDB = _NVActiveDB = new NVActiveDB(buf);
	}

	ASSERT(if (!serverlist) {
	       slog.
	       p(Logger::
		 Notice) <<
	       "CServer::CServer: serverlist is a null-pointer\n";}
	);
	// other constructors
	LServer::init(spooldir);
	RServer::init(serverlist);

	ASSERT(if (!_ServerList) {
	       slog.
	       p(Logger::
		 Notice) <<
	       "CServer::CServer: serverlist is a null-pointer\n";
	       exit(9);}
	);
	_TTLActive = 300;
	_TTLDesc = 500;
}

CServer::~CServer()
{
	VERB(slog.p(Logger::Debug) << "CServer::~CServer()\n");
	if (_OverviewFormat) {
		delete _OverviewFormat;
		_OverviewFormat = NULL;
	}
	if (_NVActiveDB) {
		delete _NVActiveDB;
		_NVActiveDB = NULL;
		_ActiveDB = NULL;
	}
}

void CServer::filter_xref(Article *art, MPListEntry *server, const char *gname)
{
	static const char delim[] = " \t\r\n";

	bool xref_changed = false;
	std::string xref = art->getfield("xref:", 1);

	std::string::size_type beg_header =
		xref.find_first_not_of(delim, 5);
	std::string::size_type end_entry =
		xref.find_first_of(delim, beg_header);
	while (end_entry != std::string::npos) {
		const std::string::size_type end_old = end_entry;
		const std::string::size_type beg_entry =
			xref.find_first_not_of(delim, end_old);

		if (beg_entry != std::string::npos) {
			const std::string::size_type colon =
				xref.find_first_of(": \t\r\n", beg_entry);
			if ((colon != std::string::npos) &&
			    (xref[colon] == ':')) {
				end_entry = xref.find_first_of(delim, colon);
				const std::string group =
					xref.substr(beg_entry,
						    colon - beg_entry);

				// check that the group is configured
				// for the specified server
				if ((gname == NULL) || (group != gname)) {
					if (_ServerList->server(group.c_str()) != server) {
						// we have to remove
						// this Xref entry
						xref_changed = true;

						if (end_entry != std::string::npos) {
							xref.erase(end_old, end_entry - end_old);
							end_entry = end_old;
						} else {
							xref.erase(end_old);
						}
					}
				}
			} else {
				end_entry = std::string::npos;
			}
		}
	}

	if (xref_changed) {
		xref.append("\r\n");
		art->setfield("Xref:", xref.c_str());
	}
}

ActiveDB *CServer::active()
{
	// The validity of the active database has to be tested twice,
	// because it is possible that several processes can reach
	// this point and in this case all these processes would
	// transfer the active database
	// Locking the database before the first active_valid call
	// is no good idea, since this requires to set a lock
	// even in cases where this is not necessary at all

	if (!active_valid()) {
		slog.p(Logger::Info) << "CServer::active: active database timed out\n";
		_NVActiveDB->lock(NVcontainer::ExclLock);
		try {
			if (!active_valid()) {
				RServer::active();
			}
		}
		catch(Error & e) {
			slog.p(Logger::Alert)
			    <<
			    "CServer::active: UNEXPECTED EXCEPTION CAUGHT, WHILE IN CRITICAL REGION!\n"
			    <<
			    "CServer::active: PLEASE REPORT TO h.straub@aon.at\n";
			e.print();
		}
		catch(...) {
			slog.p(Logger::Alert)
			    <<
			    "CServer::active: UNEXPECTED EXCEPTION CAUGHT, WHILE IN CRITICAL REGION!\n"
			    <<
			    "CServer::active: PLEASE REPORT TO h.straub@aon.at\n";
		}
		_NVActiveDB->lock(NVcontainer::UnLock);
	}
	return _NVActiveDB;
}

GroupInfo *CServer::groupinfo(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "CServer::groupinfo(" << name << ")\n");
	ASSERT(if (strlen(name) > 512) {
	       throw
	       AssertionError
	       ("CServer::groupinfo: Name of newsgroup too long\n",
		ERROR_LOCATION);}
	);

	static GroupInfo agroup;
	int errc;
	MPListEntry *mpe;

	if ((mpe = _ServerList->server(name)) == NULL) {
		throw NoSuchGroupError("no such group", ERROR_LOCATION);
	}
	if (mpe->flags & MPListEntry::F_OFFLINE)
		return LServer::groupinfo(name);

	if ((errc = _NVActiveDB->get(name, &agroup)) < 0 ||
	    agroup.mtime() + mpe->groupTimeout < nvtime(NULL)) {
		VERB(slog.p(Logger::Debug) << "CServer::groupinfo " 
				<< "groupinfo timed out\n");
		try {
			RServer::selectgroup(name, 1);
			agroup = _CurrentGroup;
		}
		catch(ResponseError & re) {
			// in case of a response error, use cached values
			if (errc < 0)
				throw NoSuchGroupError("no such group",
						       ERROR_LOCATION);
		}
		catch(SystemError & sr) {
			// in case of a system error, use cached values
			if (errc < 0)
				throw NoSuchGroupError("no such group",
						       ERROR_LOCATION);
		}
	}
	return &agroup;
}

Newsgroup *CServer::getgroup(const char *name)
{
	VERB(slog.
	     p(Logger::Debug) << "CServer::getgroup(" << name << ")\n");
	CNewsgroup *grp;
	MPListEntry *mpe;

	if ((mpe = _ServerList->server(name)) == NULL) {
		throw NoSuchGroupError("no such group", ERROR_LOCATION);
	}

	if (mpe->flags & MPListEntry::F_OFFLINE) {
		// offline => use NVNewsgroup
		GroupInfo grpinfo;
		if (_NVActiveDB->get(name, &grpinfo) < 0)
			throw NoSuchGroupError("no such group",
					       ERROR_LOCATION);
		NVNewsgroup *grp =
		    new NVNewsgroup(_OverviewFormat, _SpoolDirectory,
				    name);
		grp->setsize(grpinfo.first(), grpinfo.last());
		return grp;
	}

	if (!(mpe->flags & MPListEntry::F_CACHED)) {
		// not-cached => use RNewsgroup
		GroupInfo *grpinfo = groupinfo(name);
		if (_NVActiveDB->get(name, grpinfo) < 0)
			throw NoSuchGroupError("no such group",
					       ERROR_LOCATION);
		RNewsgroup *grp =
		    new RNewsgroup(this, _OverviewFormat, name);
		grp->setsize(grpinfo->first(), grpinfo->last());
		return grp;
	}

	GroupInfo *grpinfo = groupinfo(name);
	grp = new CNewsgroup(this, _OverviewFormat, _SpoolDirectory, name);
	grp->setsize(grpinfo->first(), grpinfo->last());
	grp->setttl(mpe->groupTimeout);
	return grp;
}

int CServer::post(Article * article)
{
	VERB(slog.p(Logger::Debug) << "CServer::post(Article*)\n");
	int r;

	if ((r = RServer::post(article)) < 0) {
		spoolarticle(article);
		return 1;
	}
#ifdef ENABLE_HEADERLOG
	char fn[MAXPATHLEN];
	fstream fs;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ssprintf(fn, sizeof(fn), "%s/.headerlog", _SpoolDirectory);
	fs.open(fn, ios::app);
	fs << getpid() << " " << tv.tv_sec << endl;
	article->write(fs, Article::Head);
	fs.close();
#endif
	return 0;
}

void CServer::spoolarticle(Article * article)
{
	VERB(slog.p(Logger::Debug) << "CServer::spoolarticle\n");

	try {
		pSpool->spoolArt(*article);
	} catch(SystemError se) {
		slog.p(Logger::Error) << "CServer::spoolarticle: catched\
		SystemError while spooling article\n";
		throw se;
	}
	catch(...) {
		slog.p(Logger::Error) << "CServer::spoolarticle: catched\
		unknown error while spooling article";
		throw;
	}
}

void CServer::postspooled(void)
{
	VERB(slog.p(Logger::Debug) << "CServer::postspooled()\n");

	Article *pA;
	int s;

	try {
		for (pA = pSpool->getSpooledArt(); pA != NULL;
		     pA = pSpool->getSpooledArt()) {
			try {
				s = RServer::post(pA);
				if (s == -1) {
					slog.p(Logger::Debug)
					    << "CServer::postspooled():\
					RServer::post returns Spooling\
					necessary; ID:" << pA->getfield("message-id") << "\n";
					pSpool->storeBadArt(*pA);
				} else if (s == -2) {
					slog.p(Logger::Error)
					    << "CServer::postspooled():\
					RServer::post returns -2; ID:" << pA->getfield("message-id") << "\n";
					pSpool->storeBadArt(*pA);
				}
			}
			catch(InvalidArticleError iae) {
				slog.p(Logger::Error)
				    << "CServer::postspooled(): RServer::post\
				throw InvalidArticleError; ID: "
				    << pA->getfield("message-id") << "\n";
				pSpool->storeBadArt(*pA);
			}
			delete pA;
		}
	}
	catch(SystemError se) {
		slog.p(Logger::Error) << "CServer::postspooled():\
			Error posting articles from spool\n";
		throw se;
	}
	catch(...) {
		slog.p(Logger::Error) << "CServer::postspooled();\
			Unknown error while posting articles\n";
		throw;
	}
}

void CServer::listgroup(const char *gname, char *lstgrp,
			unsigned int f, unsigned int l)
{
	VERB(slog.
	     p(Logger::
	       Debug) << "CServer::listgroup(" << gname << ",...)\n");
	ASSERT(if (!gname) {
	       throw
	       AssertionError
	       ("CServer::listgroup: gname parameter is a null-pointer",
		ERROR_LOCATION);}
	       if (!lstgrp) {
	       throw
	       AssertionError
	       ("CServer::listgroup: lstgrp parameter is a null-pointer",
		ERROR_LOCATION);}
	);
	MPListEntry *mpe;

	mpe = _ServerList->server(gname);
	if (mpe->flags & MPListEntry::F_OFFLINE)
		throw NSError("cannot listgroup in offline mode",
			      ERROR_LOCATION);

	RServer::listgroup(gname, lstgrp, f, l);
}

void CServer::overviewdb(Newsgroup * ng, unsigned int fst,
			 unsigned int lst)
{
	VERB(slog.p(Logger::Debug) << "CServer::overviewdb(*ng,"
	     << fst << "-" << lst << ")\n");
	MPListEntry *mpe;

	mpe = _ServerList->server(ng->name());
	if (!(mpe->flags & MPListEntry::F_OFFLINE))
		RServer::overviewdb(ng, fst, lst);
}

/* CServer::article
 * Description:
 *   Return a given article of a given newsgroup
 * Parameters:
 *   gname ... Name of the newsgroup
 *   nbr ... Number of the article
 * Return:
 *   Pointer to a statically preallocated article
 * Exceptions:
 *   SystemError ... if the connection to the news server fails
 *   NoNewsServerError ... if no news server is configured for this group
 *   NoSuchGroupError ... if the group does not exist
 *   NoSuchArticleError ... if the requested article does not exist
 *   NSError ... if in offline mode
 */
void CServer::article(const char *gname, unsigned int nbr, Article * art)
{
	VERB(slog.p(Logger::Debug) << "CServer::article(" << nbr << ")\n");
	MPListEntry *mpe;

	mpe = _ServerList->server(gname);
	if (mpe->flags & MPListEntry::F_OFFLINE)
		throw NSError("cannot retrieve articles in offline mode",
			      ERROR_LOCATION);

	RServer::article(gname, nbr, art);
	filter_xref(art, mpe, gname);
}

void CServer::article(const char *id, Article * art)
{
	VERB(slog.p(Logger::Debug) << "CServer::article(" << id << ")\n");
	char buf[1024];
	const char *p;
	string resp;

	const char * const gname =
	  (_CurrentGroup.name()[0] != '\0') ? _CurrentGroup.name() : NULL;

	snprintf(buf, sizeof(buf), "article %s\r\n", id);
	if (_CurrentServer
	    && !(_CurrentServer->flags & MPListEntry::F_OFFLINE)) {
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (strncmp(p, "220", 3) == 0) {
			art->read(*_pServerStream);
			art->setnbr(-1);
			filter_xref(art, gname ? _CurrentServer : NULL, gname);
			return;
		}
		// 412 cannot happen since we specified the article id
		// 420 -"- since we specified the article id
		// 423 -"- since we specified the article id, not the art nbr
		if (strncmp(p, "430", 3) != 0) {
			slog.p(Logger::Notice)
			    <<
			    "illegal response code to <article <id>> request\n"
			    << p;
		}
	}
	// check whether the article can be found at one of the other
	// news servers
	MPListEntry *cs = _CurrentServer;
	for (unsigned int i = 0; i < _ServerList->entries.size(); i++) {
		if (cs == &(_ServerList->entries[i]) ||
		    !_ServerList->entries[i].hostname[0] ||
		    _ServerList->entries[i].flags & MPListEntry::F_OFFLINE)
			continue;
		setserver(&(_ServerList->entries[i]));
		connect();
		resp = issue(buf, NULL);
		p = resp.c_str();
		if (strncmp(p, "220", 3) == 0) {
			art->read(*_pServerStream);
			art->setnbr(-1);
			filter_xref(art, NULL);
			return;
		}
		if (strncmp(p, "430", 3) != 0) {
			slog.p(Logger::Notice)
			    <<
			    "illegal response code to <article <id>> request\n"
			    << p;
		}
	}
	throw NoSuchArticleError(resp, ERROR_LOCATION);
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
