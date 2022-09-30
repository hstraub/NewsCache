#ifndef __NServer_h__
#define __NServer_h__
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
/* Portions Copyright (C) 2003 Herbert Straub
 * 	Implementing the ArtSpooler class
 * 	Reimplementing the complet methods:
 * 		CServer::postspooled (void)
 * 		CServer::spoolarticle (Article *article)
 */

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include <iostream>
#include <fstream>

class NServer;
class LServer;
class RServer;
class CServer;

#include "config.h"
#include "Debug.h"

#include "sockstream.h"
#include "util.h"

#include "OverviewFmt.h"
#include "ActiveDB.h"
#include "NVActiveDB.h"
#include "NVNewsgroup.h"
#include "CNewsgroup.h"
#include "Article.h"
#include "MPList.h"
#include "NSError.h"
#include "ArtSpooler.h"

/* NServer parent-class, RServer and LServer child-classes.
 */
extern char nntp_hostname[MAXHOSTNAMELEN];
extern char nntp_posting_host[MAXHOSTNAMELEN];

#ifdef EXP
/**
 * \class PostResult
 * \author Thomas Gschwind
 * \brief Post Results
 *
 * Possible errors (combinations arer possible as well!)
 * \li err+spool<sc => Message has been submitted to sc-(err+spool)
 *   servers successfully
 * \li err => # of errors
 * \li err_dup => Duplicate Message Identifier (ignore?)
 * \li err_perm || err_failed => User interaction necessary
 * \li err-err_dup-err_perm-err_failed => Internal errors
 */
class PostResult {
      public:
	MPListEntry ** server;
	int *error;
	int nservers;

	 PostResult(int sz):server(NULL), errors(NULL), nserver(0) {
		if ((server =
		     (MPListEntry **) malloc(sz *
					     sizeof(MPListEntry *))) ==
		    NULL) {
			throw
			    SystemError
			    ("cannot allocate buffer for post servers",
			     errno, ERROR_LOCATION);
		}
		if ((error = (int *) malloc(sz * sizeof(int))) == NULL) {
			free(server);
			server = NULL;
			throw
			    SystemError
			    ("cannot allocate buffer for result list",
			     errno, ERROR_LOCATION);
		}
	}

	~PostResult() {
		if (server)
			free(server);
		if (error)
			free(error);
	}

	enum { Offline, None, Duplicate, NotAllowed, PostingFailed,
		    ResponseError, SystemError, InternalError };
};
#endif
/**
 * \class NServer
 * \author Thomas Gschwind
 */
class NServer {
      protected:
	OverviewFmt * _OverviewFormat;
	ActiveDB *_ActiveDB;

	 NServer();
	/**
	* Free allocated data. Virtual because it is possible, that
	* a news server instance is destructed via the abstract
	* parent class.
	*/
	 virtual ~ NServer();
      public:
	 virtual OverviewFmt * overviewfmt() {
		return _OverviewFormat;
	} virtual ActiveDB *active() = 0;
	virtual GroupInfo *groupinfo(const char *name) = 0;
	virtual Newsgroup *getgroup(const char *name) = 0;

	//! Free the newsgroup instance allocated by getgroup.
	//! \param group pointer to newsgroup to be deleted
	virtual void freegroup(Newsgroup * group);
	virtual int post(Article * article) = 0;
};				//NServer

/** 
 * \class LServer
 * \author Thomas Gschwind
 * \brief Provides an interface to access news locally
 */
class LServer:virtual public NServer {
      protected:
	char _SpoolDirectory[MAXPATHLEN];
	ArtSpooler *pSpool;

	 LServer():NServer(), pSpool(0) {
		_SpoolDirectory[0] = '\0';
	} void init(const char *spooldir);

      public:

	//! Construct an LServer class
	//! \param spooldir Name of spool-directory
	LServer(const char *spooldir);
	virtual ~ LServer();

	//! News retrieval function
	/**
	* \return Pointer to the active database. The database pointed to by
	* the return vlue is maintained by the LServer class. Hence,
	* it should not be modified by the user and must not be
	* deleted!
	*/
	virtual ActiveDB *active();

	/**
	* \param name Name of the newsgroup
	* \return Returns informations of the newsgrup in statically
	* 	allocated GroupInfo structure.
	* \throw NoSuchGroupError If the group does not exist
	*/
	virtual GroupInfo *groupinfo(const char *name);

	/**
	* Return the newsgroup with name >name<. If the newsgroup does not
	* exist, NULL will be returned.
	* \param name Name of the newsgroup
	* \return Pointer to a Newsgroup structure. Free it by calling
	* 	the freegroup method. The newsgroup must be destructed by
	* 	the same Server-instance it was constructed from.
	* \throw NoSuchGroupError If the newsgroup does not exist
	*/

	virtual Newsgroup *getgroup(const char *name);

	/**
	* Post an article to the news server.
	* \param article The article to be posted.
	* \return 0 on success, otherwise <0
	* \bug Not yet supported
	* \todo implement this method
	*/
	virtual int post(Article * article);
};

/**
 * \class RServer
 * \author Thomas Gschwind
 * \brief Provides an interface to access news on a remote site
 */
class RServer:virtual public NServer {
      private:

	/**
	* \note
	* The news server, where the article should be posted, should be taken
	* from the article's newsgroup field. An article posted to comp.os.linux
	* should be posted to the news server from where we receive this
	* newsgroup.
	* POST:
	* 	\li 240 post ok
	* 	\li 340 send art
	* 	\li 440 not allowed
	* 	\li 441 post failed
	* \param srvr News server the article should be posted to.
	* \param article The article to be posted.
	* \throw SystemError If the connection to the news server fails.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \throw NotAllowedError If posting is prohibited.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group (in theory, this should be impossible).
	* \throw DuplicateArticleError The message id exists already on the
	* 	news server.
	* \throw PostingFailedError The server returned 441 ...
	* \throw NoSuchGroupError If the group does not exist
	* 	(no group selected).
	* 	Semantically Impossible Exceptions.
	*/
	void post(MPListEntry * srvr, Article * article);

      protected:
	MPList * _ServerList;
	MPListEntry *_CurrentServer;
	GroupInfo _CurrentGroup;

	sockstream *_pServerStream;

	RServer():NServer() {
		_ServerList = NULL;
		_CurrentServer = NULL;
		_pServerStream = NULL;
	}
	void init(MPList * serverlist);

	/**
	* Connect to the news server. Stores whether posting is allowed
	* in _CurrentServer->flags.
	* \throw SystemError ... Thrown, if a system error
	* 	(eg. cannot connect) occurs after _Retries 
	* 	successive connection-failures.
	* \throw ResponseError ... Thrown, if the news server returns a
	* 	response code different from the allowed connection 
	* 	setup return codes.
	* 	\li 200 ... reading and posting allowed
	* 	\li 201 ... reading allowed
	* \throw NoNewsServerError If no news server is configured for this
	* 	group (in theory, this should be impossible)
	* \throw NoSuchGroupError If the group does not exist
	* \todo RECONNECT/RETRY (see source code)
	*/
	void connect();

	//! Disconnect from the current news server.
	void disconnect();

	/**
	* Send a command to the remote news server and return its response.
	* \param command Command that should be sent to the news server
	* \param expresp ... Expected response code. If omitted, any response
	* 	code will do. Should be checked by the caller in this case.
	* \return static string holding the news server's response.
	* \throw SystemError If the connection fails
	* \throw ResponseError If the server returns an illegal response code
	* \throw NoNewsServerError ... if no news server is configured for
	* 	the current group.
	* \throw NoSuchGroupError If the group does not exist.
	*/
	std::string issue(const char *command, const char *expresp = NULL);

	/**
	* Disconnect from the current news server and set a new news
	* server as the current one, if the new one is different to
	* the current.
	* \param server A multiplexinglist entry pointing to the new news
	* 	server. This parameter must not be freed as long as RServer
	* 	uses this news server, since only the pointer is stored.  As
	* 	soon as RServer connects to this news server, it will store
	* 	whether posting is allowed in server->flags.
	*/
	void setserver(MPListEntry * server);

	int is_connected() {
		return (_pServerStream != NULL) ? 1 : 0;
	}

	/**
	* Select a newsgroup on the remote news server. Remarks:
	* group: 211 nflsg selected 411 no group
	* \param name Name of the newsgroup
	* \param force Force the connect to the remote server
	* \throw SystemError If the connection to the news server fails
	* \throw NoSuchGroupError ... if the group does not exist, or no
	* 	news server has been configured for this group.
	* \throw ResponseError If the news server replies an invalid response
	* 	code
	*/
	void selectgroup(const char *name, int force = 0);

      public:

	/**
	* Initialize the new RServer class. Sets up the server list and
	* the default number of retries.
	* \param serverlist list of news server and their newsgroups
	*/
	RServer(MPList * serverlist);

	virtual ~ RServer();

	/**
	* Delete all previously allocated data and install a new list of
	* news servers and newsgroups.
	* \param serverlist The list of news servers. This parameter must not
	* 	be freed as long as RServer uses this server-list,
	* 	since only the pointer is stored.
	*/
	void setserverlist(MPList * serverlist);

	/**
	* Return the currently installed list of news servers.
	* \return The server-list.
	*/
	MPList *getserverlist(void) {
		return _ServerList;
	}

	/**
	* Retrieves the active database from the news servers and returns
	* a pointer to it. 
	* If an error occurs while retrieving the database from a server,
	* the active database will not be retrieved from this server.
	* \note The offline stuff does not belong to
	* the RServer. Thus it should be moved into the CServer.  Setting
	* up all of this filter expressions can be done at the server-list's
	* creation time. However, at the moment this is neglectable. The
	* active database is retrieved rather seldom and takes a long time
	* nevertheless.
	* 
	* \return Pointer to the active database.
	*/
	virtual ActiveDB *active();

	/**
	* Returns the GroupInfo object of a given newsgroup.
	* \param name The name of newsgroup.
	* \return Returns informations of the newsgroup in a statically
	* 	allocated GroupInfo structure.
	* \throw SystemError If the connection to the news server fails
	* \throw NoSuchGroupError If the group does not exist, or no news
	* 	server has been configured for this group.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	*/
	virtual GroupInfo *groupinfo(const char *name);

	/**
	* Return the newsgroup with name >name<.  If the newsgroup does
	* not exist, NULL will be returned.
	* \param name Name of the newsgroup.
	* \return Pointer to a Newsgroup structure. Free it by calling
	* 	the freegroup method. The newsgroup must destructed by the
	* 	same Server-instance it was constructed from.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \throw SystemError ... if the connection to the news server fails.
	*/
	virtual Newsgroup *getgroup(const char *name);

	/**
	* Post an article.
	* \todo A per server result code should be returned.
	* \param article The article to be posted
	* \return 
	* 	\li 0 if the article has been posted successfully
	* 	\li -1 spooling of article is necessary
	* 	\li -2 article could be posted to some of the news servers
	* \throw InvalidArticleError If the article is not correctly formated.
	* \todo We should use the path field here, or introduce a
	* 	new field that shows the cache-posting-chain (see SourceCode).
	*/
	virtual int post(Article * article);

	/**
	* Return a list of article available in a newsgroup.
	* \param gname Name of the newsgroup.
	* \param lstgrp Pointer to a preallocated array, where the list is
	* 	stored.
	* \param f First
	* \param l Last
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	*/
	virtual void listgroup(const char *gname, char *lstgrp,
			       unsigned int f, unsigned int l);

	/**
	* Return the overviewdatabase of the newsgroup >ng<.  This function
	* should not be called by the user. Instead it should be called via
	* a Newsgroup object.
	* \param ng Name of the newsgroup.
	* \param fst First
	* \param lst Last
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \throw NotAllowedError If retrieving the overviewdb is prohibited.
	* \throw NSError If news server does not implement over and xover.
	*/
	virtual void overviewdb(Newsgroup * ng, unsigned int fst,
				unsigned int lst);

	/**
	* Return a given article of a given newsgroup
	* \note Article nbr:
	* 	\li 220 success
	* 	\li 412 no group sel
	* 	\li 420 no art sel
	* 	\li 423 art nbr not found in grp
	* 	\li 430 no such art found
	* \param gname Name of the newsgroup.
	* \param nb Number of the article.
	* \param art The article
	* \return Pointer to an article, this has to be freed by the user.
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw NoSuchArticleError If the requested article does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \bug Description and Code (Parameters) not synchronized.
	* \todo Synchronize Description and Code (Parameters).
	*/
	virtual void article(const char *gname, unsigned int nb,
			     Article * artr);

	/**
	* Return a given article of a given newsgroup
	* \note Article nbr:
	* 	\li 220 success
	* 	\li 412 no group sel
	* 	\li 420 no art sel
	* 	\li 423 art nbr not found in grp
	* 	\li 430 no such art found
	* \return Pointer to an article, this has to be freed by the user.
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw NoSuchArticleError If the requested article does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \bug Description and Code (Parameters) not synchronized.
	* \todo Synchronize Description and Code (Parameters).
	*/
	virtual void article(const char *id, Article * art);

	inline unsigned int getserverflags() {
		return _CurrentServer->flags;
	}

	inline unsigned int getnntpflags() {
		return _CurrentServer->nntpflags;
	}
};

/**
 * If the news server cannot be reached, the CServer class
 * stores the articles in its spool directory. However, this
 * class does not post these articles to the news server as soon
 * as the link is up again! Locking is done by setting an exclusive
 * lock onto the status file located which is located in the
 * spool directory.
 * \author Thomas Gschwind
 */
class CServer:virtual public LServer, public RServer {
      protected:
	NVActiveDB * _NVActiveDB;

	nvtime_t _TTLActive;
	nvtime_t _TTLDesc;
	int active_valid() {
		unsigned long tm;
		 _NVActiveDB->getmtime(&tm);
		 return (tm + _TTLActive) > nvtime(NULL);
	} int desc_valid() {
		return 1;
	}
	int group_valid();

	/**
	 * Filter the XRef header to only contain entries for groups
	 * matching the specified server.
	 */
	void filter_xref(Article *art, MPListEntry *server,
			 const char *gname = NULL);

	// This variable indicates, whether a newsgroup has been 
	// already selected on the remote news server
	// int _RSGroup;
	// Select group on remote server, if not already selected
	// int RSGroup(const char *name);

      public:

	//! \param spooldir Name of spool-directory.
	//! \param serverlist List of news servers and their newsgroups.
	CServer(const char *spooldir, MPList * serverlist);

	~CServer();

	void setttl(time_t ttl_list, time_t ttl_desc) {
		_TTLActive = ttl_list;
		_TTLDesc = ttl_desc;
	}

	// News retrieval functions
	virtual OverviewFmt *overviewfmt() {
		return _OverviewFormat;
	}

	/**
	* Retrieves the active database (list of newsgroup and the number
	* of their first and last article) from the news servers, if the
	* local copy is older than _TTLActive seconds.
	* \return ActiveDB* Pointer to the active database.
	*/
	virtual ActiveDB *active();

	/**
	 * Invalidate the Active DB. This is done by reseting the datebase
	 * timestamp value.  This is usefull, if we change the configuration
	 * and add one Server, otherwise the new server activeDB is available
	 * after the active-db Timeout (see configurations file Timeouts
	 * active-db group-description. The next call of active() retrieve the
	 * data.
	 * \author Herbert Straub
	 */
	void	invalidateActiveDB (void);

	/**
	* Returns information onto the newsgroup >name< from the active
	* database. If the last information obtained for the newsgroup is
	* older than groupTimeout (GroupTimeout in newscache.conf), 
	* a group request is issued to the news server with RServer::selectgroup().
	* If the Flag offline is specified, then the LServer::groupinfo is returned.
	* \bug Does not use the USE_EXCEPTIONS-define
	* \param name Name of the newsgroup.
	* \return Returns informations of the newsgroup in a statically
	* 	allocated GroupInfo structure.
	* \throw NoSuchGroupError If the group does not exist, or no news
	* 	server has been configured for this group.
	*/
	virtual GroupInfo *groupinfo(const char *name);

	/**
	* Return the newsgroup with name >name<. If the newsgroup does not
	* exist, NULL will be returned.
	* \param name Name of the newsgroup.
	* \return Pointer to a Newsgroup structure. Free it by calling the
	* 	freegroup method. The newsgroup must destructed by the same
	* 	Server-instance it was constructed from.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	*/
	virtual Newsgroup *getgroup(const char *name);

	/**
	* Post an article to the primary news server. The primary news server
	* is the news server being listed first in the MPList. If the article
	* cannot be submitted immediately, it is kept for later transmission.
	* \param article The article to be posted.
	* \return Status
	* 	\li 0 if the article has been posted successfully
	* 	\li 1 if the article has been spooled for later submission
	* \throw InvalidArticleError If article is not correctly formated.
	* \throw ResponseError If posting fails.
	* \throw SystemError If spooling of the article fails.
	*/
	virtual int post(Article * article);

	/**
	* Spool a news article for later transmission
	* \param article The article to be spooled.
	* \throw System Error
	*/
	virtual void spoolarticle(Article * article);

	/**
	* \author Herbert Straub
	* \date 2003
	* Post all those articles that have been queued for later transmission.
	* \throw SystemError If any error.
	*/
	virtual void postspooled(void);

	/**
	* Return a list of article available in a newsgroup
	* \author Herbert Straub
	* \date 2003
	* \param gname Name of the newsgroup.
	* \param lstgrp Pointer to a preallocated array, where the list is
	* 	stored.
	* \param f First
	* \param l Last
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \throw NSError If in offline mode.
	* \bug Description and Code (Parameters) not synchronized.
	* \todo Synchronize Description and Code (Parameters).
	*/
	virtual void listgroup(const char *gname, char *lstgrp,
			       unsigned int f, unsigned int l);

	/**
	* Return the overviewdatabase of the newsgroup >ng<.  This function
	* should not be called by the user. Instead it should be called via
	* a Newsgroup object.
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw ResponseError If the news server replies an invalid response
	* 	code.
	* \bug Description and Code (throw) not synchronized.
	* \todo Synchronize Description and Code (throw).
	*/
	virtual void overviewdb(Newsgroup * ng, unsigned int fst,
				unsigned int lst);

	/**
	* Return a given article of a given newsgroup.
	* \param gname Name of the newsgroup.
	* \param nbr Number of the article.
	* \param art The article.
	* \return Pointer to a statically preallocated article.
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw NoSuchArticleError If the requested article does not exist.
	* \throw NSError If in offline mode.
	* \bug Description and Code (return) not synchronized.
	* \todo Synchronize Description and Code (return).
	*/
	virtual void article(const char *gname, unsigned int nbr,
			     Article * art);

	/**
	* Return the article for a given article id.
	* \note Article nbr:
	* 	\li 220 success
	* 	\li 412 no group sel
	* 	\li 420 no art sel
	* 	\li 423 art nbr not found in grp
	* 	\li 430 no such art found
	* \param id Message identifier.
	* \param art Pointer to preallocated article class.
	* \throw SystemError If the connection to the news server fails.
	* \throw NoNewsServerError If no news server is configured for this
	* 	group.
	* \throw NoSuchGroupError If the group does not exist.
	* \throw NoSuchArticleError If the requested article does not exist.
	* \throw NSError If in offline mode.
	* \bug Description and Code (return) not synchronized.
	* \todo Synchronize Description and Code (return).
	*/
	virtual void article(const char *id, Article * art);
};

inline void CServer::invalidateActiveDB (void)
{
	_NVActiveDB->setmtime (0,1); // force flag 1 !!
}
#endif
