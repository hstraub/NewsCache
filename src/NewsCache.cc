/* NewsCache.cc 
 * -- (c) 1996-1998 by Thomas GSCHWIND <tom@infosys.tuwien.ac.at>
 * -- implements the cache server in combination with the NServer library
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
 * Portions Copyright (C) 2001-2003 Herbert Straub
 * Herbert Straub: modify ns_stat: bug fix stat by messageId
 * Herbert Straub: adding nice values for master and client
 * Herbert Straub: debug output in selectgroup
 */
#include "config.h"

#if !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__))
#include <crypt.h>
#endif
#include <ctype.h>
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
#include <sys/socket.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifdef HAVE_SETPRIORITY
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <limits.h>

char *ConfigurationOptions[] = {
#ifdef MD5_CRYPT
	"--with-md5",
#endif
#ifdef MD5_AUTO
	"--with-md5auto",
#endif
#ifdef SHADOW_PASSWORD
	"--with-shadow",
#endif
#ifdef PAM_AUTH
	"--with-pam="PAM_DEFAULT_SERVICENAME,
#endif
#ifdef WITH_SYSLOG
	"--with-syslog",
#endif
#ifdef ENABLE_HEADERLOG
	"--enable_headerlog",
#endif
#ifdef ENABLE_DEBUG
	"--enable-debug",
#endif
#ifdef PLAIN_TEXT_PASSWORDS
	"--enable-plainpass",
#endif
#ifdef ENABLE_ASSERTIONS
	"--enable-assertions",
#endif
#ifdef WITH_UNIQUE_PACKAGE_NAME
	"--with-uniquenames",
#endif
	NULL
};

#ifdef HAVE_LIBWRAP
#include <tcpd.h>
extern "C" int hosts_ctl(char *daemon, char *client_name,
			 char *client_addr, char *client_user);
#endif

#if (defined (MD5_AUTO) && !defined (MD5_CRYPT))
#define MD5_CRYPT
#endif				/*MD5_AUTO */

#include <unistd.h>
#include <pwd.h>
#ifdef SHADOW_PASSWORD
#include <shadow.h>
#endif				/*SHADOW_PASSWORD */
#ifdef MD5_CRYPT
#include "md5crypt.h"
#endif				/*MD5_CRYPT */
#ifdef PAM_AUTH			/*PAM_AUTH */
#include <security/pam_appl.h>
#include <grp.h>
#endif				/*PAM_AUTH */

#include <iostream>
#include <algorithm>
#include <map>
#include <string>

#include "NServer.h"
#include "Newsgroup.h"

#include "util.h"
#include "setugid.h"
#include "Config.h"
#include "Logger.h"
#include "sockstream.h"

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::for_each;
using std::ifstream;
using std::ios;
using std::istream;
using std::ofstream;
using std::ostream;
using std::map;
using std::string;
using std::vector;

/* CONF_MultiClient
 * Allow several simultaneous clients in standalone mode.
 * This switch is turned off for debug purposes mainly.
 */
#define CONF_MultiClient

//FIXME: since porting to socket++, this had to connent out
/* If you are very performance concerned, you might try to comment this
#define NO_STREAM_BUFFERING
 */

#ifndef SOCKLEN_TYPE
#define SOCKLEN_TYPE size_t
#endif

sig_atomic_t alarmed=0;
Logger slog;
const char *cmnd;
class ClientData;
// nntp helper functions
int ns_authinfo(ClientData * clt, int argc, char *argv[]);
int ns_article(ClientData * clt, int argc, char *argv[]);
int ns_stat(ClientData * clt, int argc, char *argv[]);
int ns_date(ClientData * clt, int argc, char *argv[]);
int ns_group(ClientData * clt, int argc, char *argv[]);
int ns_help(ClientData * clt, int argc, char *argv[]);
int ns_lastnext(ClientData * clt, int argc, char *argv[]);
int ns_list(ClientData * clt, int argc, char *argv[]);
int ns_listgroup(ClientData * clt, int argc, char *argv[]);
int ns_mode(ClientData * clt, int argc, char *argv[]);
int ns_newgroups(ClientData * clt, int argc, char *argv[]);
int ns_post(ClientData * clt, int argc, char *argv[]);
int ns_quit(ClientData * clt, int argc, char *argv[]);
int ns_xmotd(ClientData * clt, int argc, char *argv[]);
int ns_xover(ClientData * clt, int argc, char *argv[]);
int ns_xdebug(ClientData * clt, int argc, char *argv[]);

typedef struct nnrp_command_t {
	const char *name;
	const char *desc;
	int (*func) (ClientData *, int, char *[]);
} nnrp_command_t;

static nnrp_command_t all_nnrp_commands[] = {
	{"authinfo", "[user username|pass password]", ns_authinfo}
	,
	{"article", "[number|<id>]", ns_article}
	,
	{"head", "[number|<id>]", ns_article}
	,
	{"body", "[number|<id>]", ns_article}
	,
	{"stat", "[number|<id>]", ns_stat}
	,
	{"date", "", ns_date}
	,
	{"group", "newsgroup", ns_group}
	,
	{"help", "", ns_help}
	,
	{"last", "", ns_lastnext}
	,
	{"list",
	 "[active [wildmat]|active.times [time]|extensions|newsgroups|overview.fmt]",
	 ns_list}
	,
	{"listgroup", "[newsgroup]", ns_listgroup}
	,
	{"mode", "reader", ns_mode}
	,
	{"newgroups", "date time [GMT] [wildmat]", ns_newgroups}
	,
	{"next", "", ns_lastnext}
	,
	{"over", "[range]", ns_xover}
	,
	{"post", "", ns_post}
	,
	{"quit", "", ns_quit}
	,
	{"xdebug", "xdebug help", ns_xdebug}
	,
	{"xhdr", "header [range]", ns_xover}
	,
	{"xover", "[range]", ns_xover}
	,

	{NULL, NULL, NULL}
};


/*
 * \class NNRPCommandMap
 * \author Thomas Gschwind, Herbert Straub
 * \brief manage the commands
 */
class NNRPCommandMap:public map < string, nnrp_command_t * > {
      public:
	 /*
	  * Constructor enable all commands.
	  */
	 NNRPCommandMap();

	 /* 
	  * Enable all commands.
	  */
	 void enableAll (void);

	 /*
	  * Disable reader commands
	  * \author Herbert Straub
	  */
	 void disableRead (void);

	 /*
	  * Disable Post command.
	  * \author Herbert Straub
	  */
	 void disablePost (void);

	 /* 
	  * Disable Debug command.
	  * \author Herbert Straub
	  */
	 void disableDebug (void);
};
void set_client_command_table (ClientData &clt);

void NNRPCommandMap::enableAll (void)
{
	nnrp_command_t *p = all_nnrp_commands;
	while (p->name) {
		(*this)[p->name] = p;
		p++;
	}
}

NNRPCommandMap::NNRPCommandMap ()
{
	enableAll();
}

void NNRPCommandMap::disableRead (void)
{
	erase("article");
	erase("head");
	erase("body");
	erase("stat");
	erase("group");
	erase("last");
	erase("listgroup");
	erase("next");
	erase("over");
	erase("xhdr");
	erase("xover");
	erase("list");
	erase("mode");
	erase("newgroups");
}

void NNRPCommandMap::disablePost (void)
{
	erase("post");
}

void NNRPCommandMap::disableDebug (void)
{
	erase("xdebug");
}

static NNRPCommandMap nnrp_commands;

char config_file[MAXPATHLEN];
Config Cfg;
bool reReadConfig;

sig_atomic_t Xsignal;
int nntp_connections;

enum {
	AUTH_REQUIRED,
	AUTH_OK,
	AUTH_FAILED,
	AUTH_NOT_REQUIRED
};

/**
 * \class ClientData
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class ClientData {
      public:
	struct sockaddr_storage sock;
	SOCKLEN_TYPE socklen;
	char client_name[MAXHOSTNAMELEN];
	char client_addr[INET6_ADDRSTRLEN];
	string client_logname;
	istream *ci;
	ostream *co;

	CServer *srvr;
	char groupname[MAXGROUPNAMELEN];
	Newsgroup *grp;
	int nbr;

	AccessEntry *access_entry;
	char auth_user[512];
	char auth_pass[512];
	int auth_state;
	int auth_failures;
	int auth_max_failures;
	int auth_failure_sleep_time;

#ifdef PAM_AUTH
	pam_handle_t *pamh;
	struct pam_conv conv;
#endif

	int stat_groups;
	int stat_artingrp;
	int stat_articles;
	
	NNRPCommandMap client_command_map;

	 ClientData()
	:client_logname(""),
	    grp(NULL), nbr(-1),
	    access_entry(NULL), auth_state(AUTH_OK), auth_failures(0),
	    auth_max_failures(3), auth_failure_sleep_time(15),
	    stat_groups(0), stat_artingrp(0), stat_articles(0) {
		auth_user[0] = auth_pass[0] = '\0';
		groupname[0] = '\0';
}};

#ifdef PAM_AUTH
/* pam_conv_func by Tuomo Pyhala <tuomo.pyhala@iki.fi> */
/* Dirty kludge */
int pam_conv_func(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
  int i;
  struct pam_response *resp_array = (struct pam_response*) calloc(num_msg, sizeof(struct pam_response));
  class ClientData *clnt = (ClientData *) appdata_ptr;
  
  slog.p(Logger::Debug) << "in pam_conf_func\n";
  if(resp_array == NULL) { 
    return PAM_CONV_ERR; /* Out of memory */
  }
  for(i=0; i<num_msg; i++) {
    resp_array[i].resp_retcode = 0;
    resp_array[i].resp = NULL;
    switch(msg[i]->msg_style) {
    case PAM_PROMPT_ECHO_OFF: /* Assume this is password */
      resp_array[i].resp = strdup(clnt->auth_pass);
      break;
    case PAM_PROMPT_ECHO_ON: /* Assume this is username */
      resp_array[i].resp = strdup(clnt->auth_user);
      break;
    case PAM_ERROR_MSG:
      break;
    case PAM_TEXT_INFO:
      break;
    default:
      /* Error of some sort */
      free(resp_array);
      return PAM_CONV_ERR;
    }
  }
  *resp=resp_array;
  return PAM_SUCCESS;
}
#endif /* PAM_AUTH */

char **seekfile(const char *filename, const char *username)
{
	FILE *F;
	static char buf[1024];
	char *p;
	int i;

	static char *fields[8];
	int fields_n = sizeof(fields) / sizeof(fields[0]);

	if ((F = fopen(filename, "r")) == NULL) {
		slog.
		    p(Logger::
		      Error) << "cannot fopen " << filename << "\n";
		return NULL;
	}

	while (fgets(buf, sizeof(buf), F) != NULL) {
		p = buf + strlen(buf) - 1;
		while (p > buf && isspace(*p))
			*p-- = '\0';
		p = buf;
		while (isspace(*p))
			p++;
		if (*p == '#' || *p == '\0')
			continue;

		fields[0] = p;
		for (i = 1; *p && i < fields_n - 1; p++) {
			if (*p == ':') {
				*p = '\0';
				fields[i++] = p + 1;
			}
		}
		fields[i] = NULL;

		if (strcmp(username, fields[0]) == 0) {
			(void) fclose(F);
			return fields;
		}
	}
	(void) fclose(F);
	return NULL;
}

int auto_cryptcheck(const char *key, const char *pass)
{
#ifdef MD5_AUTO
	if (strlen(pass) == 24 && pass[23] == '=' && pass[22] == '=') {
		// looks like an MD5 hash
#endif
#ifdef MD5_CRYPT
		return strcmp(pass, (const char *) md5crypt(key));
#endif
#ifdef MD5_AUTO
	} else {
#endif
		return strcmp(pass, (const char *) crypt(key, pass));
#ifdef MD5_AUTO
	}
#endif				/*MD5_AUTO */
	return 0;
}

int check_auth_unix (ClientData *clt)
{
	AccessEntry *access_entry = clt->access_entry;
	string type("unix:");

	if (access_entry->authentication.typeEqual (type))
		return -1;

	const char *pass;
	struct passwd *pw = NULL;
	pw = getpwnam(clt->auth_user);
	if (!pw || !(pw->pw_name) || !(pw->pw_passwd)
	    || !(pw->pw_passwd[0]))
		return -1;
	pass = pw->pw_passwd;
#ifdef SHADOW_PASSWORD
	if ((pass[0] == 'x') && (pass[1] == '\0')) {
		struct spwd *spw = NULL;
		spw = getspnam(pw->pw_name);
		if (!(spw) || !(spw->sp_namp) || !(spw->sp_pwdp))
			return -1;
		pass = spw->sp_pwdp;
	}
#endif
	if (auto_cryptcheck(clt->auth_pass, pass))
		return -1;

	access_entry->list |= NewsgroupFilter(
			(access_entry->authentication.getField(1))
			.c_str());
	access_entry->read |= NewsgroupFilter(
			(access_entry->authentication.getField(2))
			.c_str());
	access_entry->postTo |= NewsgroupFilter(
			(access_entry->authentication.getField(3)).
			c_str());
	access_entry->modifyAccessFlags (access_entry->authentication.
			getField(4));
	slog.p(Logger::Debug) << "check_auth_unix::NewsgroupFilter set to: "
		<< access_entry->list.getRulelist() << ":"
		<< access_entry->read.getRulelist() << ":"
		<< access_entry->postTo.getRulelist() << "\n";

	return 0;
}

int check_auth_file (ClientData *clt)
{
	AccessEntry *access_entry = clt->access_entry;
	string type("file:");

	if (access_entry->authentication.typeEqual (type))
		return -1;

	string authfile = access_entry->authentication.getField(1);
	char **authv = seekfile(authfile.c_str(), clt->auth_user);
	int authc = 0;
	int passf=-1;

	if (!authv)
		return -1;

	while (authv[authc])
		authc++;
	if (authc < 3)
		return -1;


#ifdef PLAIN_TEXT_PASSWORDS
	passf=strcmp(clt->auth_pass, authv[1]);
#endif
	if (passf)
		passf=auto_cryptcheck(clt->auth_pass, authv[1]);
	if (passf)
		return -1;

	// Now we build a authentication string like the 
	// unix:::: authentication string and set authentication
	// Herbert Straub
	access_entry->authentication.set (type+authfile);
	for (authc=2; authv[authc]; authc++) {
		access_entry->authentication.appendField(authv[authc]);
	}
	access_entry->list |= NewsgroupFilter(
			(access_entry->authentication.getField(2))
			.c_str());
	access_entry->read |= NewsgroupFilter(
			(access_entry->authentication.getField(3))
			.c_str());
	access_entry->postTo |= NewsgroupFilter(
			(access_entry->authentication.getField(4)).
			c_str());
	access_entry->modifyAccessFlags (access_entry->authentication.
			getField(5));
	slog.p(Logger::Debug) << "check_auth_file::NewsgroupFilter set to: "
		<< access_entry->list.getRulelist() << ":"
		<< access_entry->read.getRulelist() << ":"
		<< access_entry->postTo.getRulelist() << "\n";
	
	return 0;
}

#ifdef PAM_AUTH
int check_pam_user_pass (ClientData *clt)
{
	int retval;

	clt->conv.appdata_ptr = clt;
	clt->conv.conv = pam_conv_func;
	clt->pamh = NULL;
	retval =
	    pam_start(clt->access_entry->PAMServicename.c_str(), clt->auth_user,
		      &(clt->conv), &(clt->pamh));
	slog.p(Logger::Debug) << "check_pam_user_pass: pam_start returns " 
		<< retval << "\n";
	if (retval == PAM_SUCCESS) {
		retval = pam_authenticate(clt->pamh, 0);	/* is user really user? */
		slog.p(Logger::Debug) << "check_pam_user_pass: pam_authenticate returns " 
			<< retval << "\n";
	}
	if (retval == PAM_SUCCESS) {
		retval = pam_acct_mgmt(clt->pamh, 0);	/* permitted access? */
		slog.p(Logger::Debug) << "check_pam_user_pass: pam_acct_mgmt returns " 
			<< retval << "\n";
	}
	if (retval != PAM_SUCCESS) {
		pam_end(clt->pamh, retval);
		return -1;
	}
	pam_end(clt->pamh, retval);

	return 0;
}
int check_auth_pam_file (ClientData *clt)
{
/*
 * check_auth_pam_file code by Tuomo Pyhala <tuomo.pyhala@iki.fi>
 * Modify by Herbert Straub <h.straub@aon.at>:
 * 	using class Authentication
 * 	restructure source code
 */
	slog.p(Logger::Debug) << "in check_out_pam_file\n";
	AccessEntry *access_entry = clt->access_entry;
	string type("pam+file:");

	if (access_entry->authentication.typeEqual (type))
		return -1;

	if (check_pam_user_pass (clt))
		return -1;

	Authentication userAuth, defAuth;
	string authfile = access_entry->authentication.getField(1);
	char **authv = seekfile(authfile.c_str(), clt->auth_user);
	if (authv == NULL)
		return -1;

	int authc, j;
	struct group *dummy_group;
	char group_entry[1024];

	userAuth.set (type+authfile);
	for (authc=1; authv[authc]; authc++) {
		userAuth.appendField (authv[authc]);
	}
	
	/* apply rights of user '*' to everyone :) */
	authv = seekfile(authfile.c_str(), "*");
	if (authv != NULL) {
		defAuth.set (type+authfile);
		for (authc=1; authv[authc]; authc++)
			defAuth.appendField (authv[authc]);
	}

	/* Gather rights given to user supplementary groups */
	vector<Authentication> groupAuths;
	setgrent();
	while ((dummy_group = getgrent()) != NULL) {
		for (j = 0; dummy_group->gr_mem[j]; j++) {
			if (!strcmp (dummy_group->gr_mem[j], clt->auth_user)) {
				group_entry[0] = '@';
				strcpy(group_entry + 1,
				       dummy_group->gr_name);
				authv = seekfile(authfile.c_str(),
					     group_entry);
				if (authv != NULL) {
					Authentication a;
					a.set(type+authfile);
					for (authc = 1; authv[authc]; authc++) {
						a.appendField(authv[authc]);
					}
					groupAuths.push_back(a);
				}
				break;
			}
		}
	}
	// Now assemble the various authentication informations.
	access_entry->authentication = userAuth;
	access_entry->authentication.modify (2, defAuth);
	vector<Authentication>::iterator ap, ae;
	ap = groupAuths.begin();
	ae = groupAuths.end();
	for (; ap!=ae; ap++) {
		access_entry->authentication.modify (2, *ap);
	}
	// and set the new access_entry values
	access_entry->list |= NewsgroupFilter(
			(access_entry->authentication.getField(2))
			.c_str());
	access_entry->read |= NewsgroupFilter(
			(access_entry->authentication.getField(3))
			.c_str());
	access_entry->postTo |= NewsgroupFilter(
			(access_entry->authentication.getField(4)).
			c_str());
	access_entry->modifyAccessFlags (access_entry->authentication.
			getField(5));
	slog.p(Logger::Debug) << "check_auth_pam_file::NewsgroupFilter set to: "
		<< access_entry->list.getRulelist() << ":"
		<< access_entry->read.getRulelist() << ":"
		<< access_entry->postTo.getRulelist() << "\n";
	
	return 0;
}

int check_auth_pam (ClientData *clt)
{
	AccessEntry *access_entry = clt->access_entry;
	string type("pam:");

	if (access_entry->authentication.typeEqual(type))
		return -1;

	if (check_pam_user_pass (clt))
		return -1;

	access_entry->list |= NewsgroupFilter(
			(access_entry->authentication.getField(1))
			.c_str());
	access_entry->read |= NewsgroupFilter(
			(access_entry->authentication.getField(2))
			.c_str());
	access_entry->postTo |= NewsgroupFilter(
			(access_entry->authentication.getField(3)).
			c_str());
	access_entry->modifyAccessFlags (access_entry->authentication.
			getField(4));
	slog.p(Logger::Debug) << "check_auth_pam::NewsgroupFilter set to: "
		<< access_entry->list.getRulelist() << ":"
		<< access_entry->read.getRulelist() << ":"
		<< access_entry->postTo.getRulelist() << "\n";

	return 0;
}
#endif /* PAM_AUTH */

/* the following formats may be specified (file/unix)
 * list:read:postto:allow
 */
int check_authentication(ClientData * clt)
{
	AccessEntry *access_entry = clt->access_entry;

	if (access_entry->authentication.getType() == "none") {
		return -1;
	}

	if (!strncmp((access_entry->authentication.getType()).c_str()
				, "unix", 4)) {
		return check_auth_unix (clt);
	} else
	    if (!strncmp((access_entry->authentication.getType()).c_str(),
				    "file", 4))
	{
		return check_auth_file (clt);
	}
#ifdef PAM_AUTH
	else if (!strncmp
		 ((access_entry->authentication.getType()).c_str(),
		  "pam+file", 8)) {
		return check_auth_pam_file (clt);
	}
	else if (!strncmp
		 ((access_entry->authentication.getType()).c_str(),
		  "pam", 3)) {
		return check_auth_pam (clt);
	}
#endif				/* #ifndef PAM_AUTH */
	// auth_{deny,none} is handled in nnrpd()
	return -1;
}


GroupInfo *selectgroup(ClientData * clt, const char *group)
{
	GroupInfo *gi;
	VERB(slog.
	     p(Logger::
	       Debug) << "NewsCache selectgroup (" << group << ")\n");

	try {
		gi = clt->srvr->groupinfo(group);
	} catch(const NSError & nse) {
		return NULL;
	}
	catch(...) {
		return NULL;
	}
	// Reset Article Pointer
	if (gi->first() <= gi->last())
		clt->nbr = gi->first();
	else
		clt->nbr = -1;
	if (strcmp(clt->groupname, group) != 0) {
		if (clt->grp)
			clt->srvr->freegroup(clt->grp);
		clt->grp = NULL;
		clt->stat_groups++;
		if (clt->stat_artingrp) {
			slog.p(Logger::Notice) << clt->
			    client_logname << " group " << clt->
			    groupname << " " << clt->stat_artingrp << "\n";
			clt->stat_artingrp = 0;
		}
		strcpy(clt->groupname, group);
	} else {
		//FIXME! NServer has to maintain a list of all the allocated
		//FIXME! newsgroups and should call this function itself!!!
		if (clt->grp)
			clt->grp->setsize(gi->first(), gi->last());
	}
	return gi;
}

void nsh_particle(ClientData * clt, char *cmnd, int nbr, Article * a)
{
	string aid = a->getfield("Message-ID:");

	switch (cmnd[0]) {
	case 'a':
		(*clt->co) << "220 " << nbr << " " << aid
		    << " article retrieved - head and body follow\r\n";
		(*clt->co) << *a << ".\r\n";
		slog.p(Logger::Info) << "articlesize " << clt->
		    groupname << ":" << nbr << " " << a->length() << "\n";
		break;
	case 'h':
		(*clt->co) << "221 " << nbr << " " << aid
		    << " article retrieved - head follows\r\n";
		a->write(*clt->co, Article::Head);
		(*clt->co) << ".\r\n";
		break;
	case 'b':
		(*clt->co) << "222 " << nbr << " " << aid
		    << " article retrieved - body follows\r\n";
		a->write(*clt->co, Article::Body);
		(*clt->co) << ".\r\n";
		slog.p(Logger::Info) << "articlesize " << clt->
		    groupname << ":" << nbr << " " << a->length() << "\n";
		break;
	}
}


#define REJECT_SEQUENCE 		"502"
#define PASS_NEEDED_SEQUENCE 		"381"
#define OK_SEQUENCE 			"281"
#define INCORRECT_ORDER_SEQUENCE 	"482"

//Authinfo
int ns_authinfo(ClientData * clt, int argc, char *argv[])
{
	// check arguments
	if (argc != 3)
		goto ns_authinfo_error;
	if (strlen(argv[2]) > sizeof(clt->auth_user) - 1) {
		(*clt->co) << REJECT_SEQUENCE " argument too long\r\n";
		return -1;
	}

	if (!strcasecmp(argv[1], "user")) {
		strcpy(clt->auth_user, argv[2]);
		(*clt->co) << PASS_NEEDED_SEQUENCE " password please\r\n";
		return 0;
	}

	if (!strcasecmp(argv[1], "pass")) {
		if (clt->auth_user[0] == '\0') {
			(*clt->
			 co) << INCORRECT_ORDER_SEQUENCE
		      " issue authinfo user first\r\n";
			return -1;
		}
		strcpy(clt->auth_pass, argv[2]);

		if (check_authentication(clt) < 0) {
			sleep (clt->auth_failure_sleep_time*
					(++clt->auth_failures));
			(*clt->
			 co) << REJECT_SEQUENCE << " Bad authinfo.\r\n";
			return -1;
		}

		(*clt->co) << OK_SEQUENCE << " OK.\r\n";
		clt->auth_state = AUTH_OK;
		set_client_command_table(*clt);

		return 0;
	}
	// error
      ns_authinfo_error:
	if (argc > 1) {
		switch (tolower(argv[1][0])) {
		case 'u':
			(*clt->
			 co) << "501 Syntax: authinfo user username\r\n";
			return -1;
		case 'p':
			(*clt->
			 co) << "501 Syntax: authinfo pass password\r\n";
			return -1;
		}
	}
	(*clt->
	 co) << "501 Syntax: authinfo [user username|pass password]\r\n";
	return -1;
}

int ns_article(ClientData * clt, int argc, char *argv[])
{
	if (argc > 2)
		goto ns_article_error;

	// retrieve article|body|head by number?
	if (argc == 1 || isdigit(argv[1][0])) {
		if (clt->groupname[0] == '\0') {
			(*clt->
			 co) << "412 no newsgroup has been selected\r\n";
			return -1;
		}
		try {
			if (!clt->grp)
				clt->grp =
				    clt->srvr->getgroup(clt->groupname);
		}
		catch(const NoSuchGroupError & nsge) {
			(*clt->co) << "411 no such newsgroup\r\n";
			return -1;
		}
		catch(const Error & e) {
			(*clt->co) << "412 operation failed\r\n";
			return -1;
		}

		if (argc == 1 && clt->nbr < 0) {
			(*clt->
			 co) <<
		      "420 no current article has been selected\r\n";
			return -1;
		}

		int nbr = (argc == 2) ? atoi(argv[1]) : clt->nbr;

		ASSERT2(clt->grp->testdb());
		Article *a;
		
		if ((a = clt->grp->getarticle(nbr)) == NULL) {
			(*clt->
			 co) <<
		      "423 no such article number in this group\r\n";
			return -1;
		}
		nsh_particle(clt, argv[0], nbr, a);
		clt->grp->freearticle(a);
		clt->nbr = nbr;
		clt->stat_artingrp++;
		clt->stat_articles++;
		return 0;
	}
	// retrieve article|body|head by id?
	if (argv[1][0] == '<') {
		try {
			Article a;
			clt->srvr->article(argv[1], &a);
			nsh_particle(clt, argv[0], 0, &a);
		}
		catch(const NoSuchArticleError &nsae) {
			(*clt->co) << "430 no such article id found\r\n";
			return -1;
		}
		catch(const ResponseError &e) {
			// error
			(*clt->co) << e._got << "\r\n";
			return -1;
		}
		catch(const Error &e) {
			// error
			(*clt->co) << "520 ???\r\n";
			slog.
			    p(Logger::
			      Error) <<
			    "Internal Error? Please report to h.straub@aon.at\r\n";
			e.print();
			return -1;
		}
		clt->stat_artingrp++;
		clt->stat_articles++;
		return 0;
	}
	// error
      ns_article_error:
	switch (argv[0][0]) {
	case 'a':
		(*clt->co) << "501 Syntax: article [nbr|<id>]\r\n";
		return -1;
	case 'h':
		(*clt->co) << "501 Syntax: head [nbr|<id>]\r\n";
		return -1;
	case 'b':
		(*clt->co) << "501 Syntax: body [nbr|<id>]\r\n";
		return -1;
	}
	return -1;
}

int ns_stat(ClientData * clt, int argc, char *argv[])
{
	/* Offene Punkte:
	   ** stat auf nummer die nicht vorhanden ist, ergibt:
	   ** 223 351  status
	   ** das ist falsch, sollte sein:
	   ** 423 No Such Article In Group
	   **
	 */
	if (argc > 2)
		goto ns_stat_error;

	// stat by number?
	if (argc == 1 || isdigit(argv[1][0])) {
		// fetch article by number
		if (clt->groupname[0] == '\0') {
			(*clt->
			 co) << "412 no newsgroup has been selected\r\n";
			return -1;
		}
		try {
			if (!clt->grp)
				clt->grp =
				    clt->srvr->getgroup(clt->groupname);
		}
		catch(const NoSuchGroupError & nsge) {
			(*clt->co) << "411 no such newsgroup\r\n";
			return -1;
		}
		catch(const Error & e) {
			(*clt->co) << "412 operation failed\r\n";
			return -1;
		}

		if (argc == 1 && clt->nbr < 0) {
			(*clt->
			 co) <<
		      "420 no current article has been selected\r\n";
			return -1;
		}

		int nbr = (argc == 2) ? atoi(argv[1]) : clt->nbr;

		ASSERT2(clt->grp->testdb());
		OverviewFmt *of = clt->srvr->overviewfmt();
		const char *o = clt->grp->getover(nbr);
		(*clt->co) << "223 " << nbr << " " << of->getfield(o,
								   "Message-ID:",
								   0)
		    << " status \r\n";
		return 0;
	}
	// stat by id?
	if (argv[1][0] == '<') {
		//(*clt->co) << "223 0 " << argv[1] << " status\r\n";
		Article a;
		try {
			clt->srvr->article(argv[1], &a);
			//nsh_particle(clt, argv[0], 0, &a);
		}
		catch(const NoSuchArticleError &nsae) {
			(*clt->co) << "430 no such article id found\r\n";
			return -1;
		}
		catch(const ResponseError &e) {
			// error
			(*clt->co) << e._got << "\r\n";
			return -1;
		}
		catch(const Error &e) {
			// error
			(*clt->co) << "520 ???\r\n";
			slog.
			    p(Logger::
			      Error) <<
			    "Internal Error? Please report to h.straub@aon.at\r\n";
			e.print();
			return -1;
		}
		string mid = a.getfield("Message-ID:");
		(*clt->co) << "223 0 " << mid << " status\r\n";
		return 0;
	}
	// error
      ns_stat_error:
	(*clt->co) << "501 Syntax: stat [nbr|<id>]\r\n";
	return -1;
}

/**
 * \author Thomas Gschwind
 * Returns the gmtime. See RFC2980 DATE command.
 * \returns 0 on success, -1 on error
 */
int ns_date(ClientData * clt, int argc, char *argv[])
{
	struct tm *ltm;
	time_t conv;
	char buf[256];

	if (argc != 1) {
		(*clt->co) << "501 Syntax: group Newsgroup\r\n";
		return -1;
	}
	time(&conv);
	if ((ltm = gmtime(&conv)) == NULL) {
		(*clt->co) << "500 gmtime failed\r\n";
		slog.p(Logger::Error) << "ns_date() gmtime failed\n";
		return -1;
	}
	sprintf(buf, "111 %04d%02d%02d%02d%02d%02d\r\n",
		1900 + ltm->tm_year, ltm->tm_mon + 1, ltm->tm_mday,
		ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
	(*clt->co) << buf;

	return 0;
}

/**
 * \author Thomas Gschwind
 * Find the news server responsible for this newsgroup.
 * Connect to it and select the newsgroup.
 */
int ns_group(ClientData * clt, int argc, char *argv[])
{
	if (argc != 2) {
		(*clt->co) << "501 Syntax: group Newsgroup\r\n";
		return -1;
	}

	if (clt->access_entry->read.matches(argv[1]) <= 0) {
		(*clt->co) << "480 authentication required\r\n";
		return 0;
	}

	GroupInfo *gi;
	if ((gi = selectgroup(clt, argv[1])) == NULL) {
		(*clt->co) << "411 no such news group\r\n";
		return -1;
	}

	(*clt->co) << "211 "
	    << gi->n() << " "
	    << gi->first() << " "
	    << gi->last() << " " << gi->name() << " group selected\r\n";

	slog.p(Logger::Info) << "groupsize [" << gi->first() << ","
	    << gi->last() << "]\n";

	return 0;
}

/**
 * \author Thomas Gschwind
 * Lists the available commands.
 */
int ns_help(ClientData * clt, int argc, char *argv[])
{
	map < string, nnrp_command_t * >::iterator begin =
	    clt->client_command_map.begin(), end = clt->client_command_map.end();

	VERB(slog.p(Logger::Debug) << "NewsCache::ns_help:\n");
	(*clt->co) << "100 Legal commands\r\n";
	while (begin != end) {
		if (begin->second->func) {
			(*clt->co) << "  " << begin->
			    first << " " << begin->second->desc << "\r\n";
		}
		++begin;
	}
	(*clt->co) << "Report problems to " << Cfg.Admin << "\r\n.\r\n";
	return 0;
}

/* last,next */
int ns_lastnext(ClientData * clt, int argc, char *argv[])
{
	if (argc != 1) {
		switch (argv[0][0]) {
		case 'l':
			(*clt->co) << "501 Syntax: last\r\n";
			return -1;
		case 'n':
			(*clt->co) << "501 Syntax: next\r\n";
			return -1;
		}
	}
	if (clt->groupname[0] == '\0') {
		(*clt->co) << "412 No newsgroup currently selected\r\n";
		return -1;
	}
	if (clt->nbr < 0) {
		(*clt->
		 co) << "420 no current article has been selected\r\n";
		return -1;
	}
	try {
		if (!clt->grp)
			clt->grp = clt->srvr->getgroup(clt->groupname);
	}
	catch(const NoSuchGroupError & nsge) {
		(*clt->co) << "411 no such news group\r\n";
		return -1;
	}
	catch(const Error & e) {
		(*clt->co) << "412 operation failed\r\n";
		return -1;
	}

	unsigned int f, i = clt->nbr, l;
	const char *o;
	string aid;
	OverviewFmt *ofmt = clt->srvr->overviewfmt();
	clt->grp->getsize(&f, &l);
	switch (argv[0][0]) {
	case 'l':
		for (;;) {
			i--;
			if (i < f) {
				(*clt->
				 co) << "422 No previous to retrieve\r\n";
				return -1;
			}
			o = clt->grp->getover(i);
			if (o[0]) {
				aid = ofmt->getfield(o, "Message-ID:", 0);
				break;
			}
		}
		break;
	case 'n':
		for (;;) {
			++i;
			if (i > l) {
				(*clt->
				 co) << "421 No next to retrieve\r\n";
				return -1;
			}
			o = clt->grp->getover(i);
			if (o[0]) {
				aid = ofmt->getfield(o, "Message-ID:", 0);
				break;
			}
		}
	}
	clt->nbr = i;

	(*clt->co) << "223 " << clt->nbr << " " << aid
	    << " article retrieved - request text separately\r\n";
	return 0;
}

class print_list_active {
	ostream & os;
      public:
	print_list_active(ostream & os):os(os) {
	} void operator() (const GroupInfo & gd) {
		os << gd << "\r\n";
		if (!os.good())
			throw SystemError("cannot write to clientstream",
					  errno, ERROR_LOCATION);
	}
};

class print_list_active_times {
	ostream & os;
      public:
	print_list_active_times(ostream & os):os(os) {
	} void operator() (const GroupInfo & gd) {
		os << gd.name() << " " << gd.ctime() << " news\r\n";
		if (!os.good())
			throw SystemError("cannot write to clientstream",
					  errno, ERROR_LOCATION);
	}
};

template < class Format > class active_filter_group {
	Format fmt;
	const NewsgroupFilter & filter;
      public:
	active_filter_group(ostream & os, const NewsgroupFilter & filter)
	:fmt(os), filter(filter) {
	}
	void operator() (const GroupInfo & gd) {
		if (filter.matches(gd.name()) > 0)
			fmt(gd);
	}
};

template < class Format > class active_filter_group_time {
	Format fmt;
	const NewsgroupFilter & filter;
	nvtime_t ctime;
      public:
	active_filter_group_time(ostream & os,
				 const NewsgroupFilter & filter,
				 nvtime_t ctime)
	:fmt(os), filter(filter), ctime(ctime) {
	}
	void operator() (const GroupInfo & gd) {
		if (gd.ctime() > ctime && filter.matches(gd.name()) > 0)
			fmt(gd);
	}
};

template < class Format > class active_filter_time {
	Format fmt;
	nvtime_t ctime;
      public:
	active_filter_time(ostream & os, nvtime_t ctime)
      :    fmt(os), ctime(ctime) {
	}
	void operator() (const GroupInfo & gd) {
		if (gd.ctime() > ctime)
			fmt(gd);
	}
};

/* List [active|newsgroups|overview.fmt]
 * List the specified data
 */
int ns_list(ClientData * clt, int argc, char *argv[])
{
	if (argc <= 3) {
		if (argc == 1 || strcasecmp(argv[1], "active") == 0) {
			ActiveDB *active;
			active = clt->srvr->active();
			(*clt->
			 co) <<
		      "215 List of newsgroups (Group High Low Flags) follows\r\n";
			try {
				NewsgroupFilter filter(clt->access_entry->
						       list);
				if (argc == 3)
					filter.setWildmat(argv[2]);
				for_each(active->begin(),
					 active->end(),
					 active_filter_group <
					 print_list_active > (*clt->co,
							      filter));
			}
			catch(const SystemError & se) {
				return -2;
			}
			(*clt->co) << ".\r\n";
			return 0;
		} else if (strcasecmp(argv[1], "active.times") == 0) {
			ActiveDB *active;
			active = clt->srvr->active();
			(*clt->co) << "215 information follows\r\n";
			try {
				NewsgroupFilter filter(clt->access_entry->
						       list);
				if (argc == 3)
					filter &= NewsgroupFilter(argv[2]);

				for_each(active->begin(),
					 active->end(),
					 active_filter_group <
					 print_list_active_times >
					 (*clt->co, filter));
			}
			catch(const SystemError & se) {
				return -2;
			}
			(*clt->co) << ".\r\n";
			return 0;
		} else if (argc == 2) {
			if (strcasecmp(argv[1], "extensions") == 0) {
				(*clt->
				 co) << "202 extensions supported\r\n";
				(*clt->co) << " over\r\n";
				(*clt->co) << " authinfo user\r\n";
				(*clt->co) << ".\r\n";
				return 0;
			} else if (strcasecmp(argv[1], "newsgroups") == 0) {
				char buf[MAXPATHLEN];
				string strg;
				sprintf(buf, "%s/.newsgroups",
					Cfg.SpoolDirectory);
				ifstream ifs(buf);
				(*clt->
				 co) << "215 Description follows\r\n";
				for (;;) {
					nlreadline(ifs, strg);
					if (!ifs.good())
						break;
					(*clt->co) << strg;
				}
				(*clt->co) << ".\r\n";
				return 0;
			} else if (strcasecmp(argv[1], "overview.fmt") ==
				   0) {
				(*clt->
				 co) <<
			      "215 Order of fields in overview database\r\n"
			      << *(clt->srvr->overviewfmt()) << ".\r\n";
				return 0;
			}
		}
	}

	(*clt->
	 co) <<
      "501 Syntax: list [active [wildmat]|active.times [time]|extensions|newsgroups|overview.fmt]\r\n";
	return -1;
}

/* listgroup
 * Listgroup only works for groups, where the overview
 * database has already been cached!
 */
int ns_listgroup(ClientData * clt, int argc, char *argv[])
{
	if (argc > 2) {
		(*clt->co) << "501 Syntax: listgroup [Newsgroup]\r\n";
		return -1;
	}
	if (argc == 1 && clt->groupname[0] == '\0') {
		(*clt->co) << "412 Not currently in a newsgroup\r\n";
		return -1;
	}
	if (argc == 2) {
		if (selectgroup(clt, argv[1]) == NULL) {
			(*clt->co) << "411 no such news group\r\n";
			return -1;
		}
		if (clt->access_entry->read.matches(argv[1]) <= 0) {
			(*clt->co) << "480 authentication required\r\n";
			return 0;
		}
	}

	try {
		if (!clt->grp)
			clt->grp = clt->srvr->getgroup(clt->groupname);
	}
	catch(const NoSuchGroupError & nsge) {
		(*clt->co) << "411 no such news group\r\n";
		return -1;
	}
	catch(const Error & e) {
		(*clt->co) << "412 operation failed\r\n";
		e.print();
		return -1;
	}

	(*clt->co) << "211 list of article numbers follow\r\n";
	clt->grp->printlistgroup(*clt->co);
	(*clt->co) << ".\r\n";
	return 0;
}

int ns_mode(ClientData * clt, int argc, char *argv[])
{
	if (argc != 2 ||
	    (strcasecmp(argv[1], "reader") != 0 &&
	     strcasecmp(argv[1], "query") != 0)) {
		(*clt->co) << "501 Syntax: mode (reader|query)\r\n";
		return -1;
	}
	(*clt->
	 co) << "200 " PACKAGE " " VERSION ", accepting NNRP commands\r\n";
	return 0;
}

/* 
 * NEWGROUPS date time [GMT|UTC] [<distributions>]
 */
#define DD2INT(x) ((x)[0]-'0')*10+((x)[1]-'0')
int ns_newgroups(ClientData * clt, int argc, char *argv[])
{
	if (argc < 3 || argc > 5) {
	      ns_newgroups_error:
		(*clt->
		 co) <<
	      "501 Syntax:  NEWGROUPS date time [GMT] [<wildmat>]\r\n";
		return -1;
	}

	int i, y4, is_gmt = 0;
	struct tm ltm;
	time_t lt;
	char *p;

	// check first two arguments, y4 indicates whether year has 4 digits
	for (i = 0; isdigit(argv[2][i]); i++);
	if (i != 6 || argv[2][i])
		goto ns_newgroups_error;
	for (y4 = 0; isdigit(argv[1][y4]); y4++);
	if ((y4 != 8 && y4 != 6) || argv[1][y4])
		goto ns_newgroups_error;
	y4 -= 4;

	// check whether time/date is specified in GMT
	if (argc >= 4) {
		if (strcasecmp(argv[3], "gmt") != 0 &&
		    strcasecmp(argv[3], "utc") != 0) {
			goto ns_newgroups_error;
		}
		is_gmt = 1;
	}
	// fill in ltm with supplied values
	p = argv[1];
	if (y4 > 2) {
		i = DD2INT(p);
		p += 2;
		ltm.tm_year = i * 100 + DD2INT(p) - 1900;
		p += 2;
	} else {
		struct tm *now;
		int yr;

		// draft-ietf-nntpext-base-04.txt:
		// If the first two digits of the year are not specified,
		// the year is to be taken from the current century if YY
		// is smaller than or equal to the current year,
		// otherwise the year is from the previous century.
		time(&lt);
		now = is_gmt ? gmtime(&lt) : localtime(&lt);
		yr = now->tm_year % 100;

		ltm.tm_year = DD2INT(p);
		p += 2;
		if (ltm.tm_year > yr)
			ltm.tm_year += now->tm_year - yr - 100;
		else
			ltm.tm_year += now->tm_year - yr;
	}
	ltm.tm_mon = DD2INT(p) - 1;
	p += 2;			// tm_mon has a range of 0(Jan)--11(Dec)!!!
	ltm.tm_mday = DD2INT(p);

	p = argv[2];
	ltm.tm_hour = DD2INT(p);
	p += 2;
	ltm.tm_min = DD2INT(p);
	p += 2;
	ltm.tm_sec = DD2INT(p);

#ifdef HAVE_TZNAME
	ltm.tm_isdst = daylight;

	lt = mktime(&ltm);
	if (lt == -1)
		goto ns_newgroups_error;

	// correct timezone if time/date is given in GMT
	if (is_gmt) {
		lt -= timezone;
	}
#else
	struct tm *plt;

	time(&lt);
	plt = localtime(&lt);

	ltm.tm_isdst = plt->tm_isdst;
	ltm.tm_gmtoff = plt->tm_gmtoff;

	lt = mktime(&ltm);
	if (lt == -1)
		goto ns_newgroups_error;
#endif

	ActiveDB *active;
	try {
		active = clt->srvr->active();
	}
	catch(const Error & e) {
		(*clt->co) << "410 operation failed\r\n";
		return -1;
	}
	(*clt->co) << "231 list of new groups follows " << lt << "\r\n";
	try {
		NewsgroupFilter filter();

		for_each(active->begin(),
			 active->end(),
			 active_filter_group_time <
			 print_list_active_times > (*clt->co,
						    clt->access_entry->
						    list, lt));
	}
	catch(const SystemError & se) {
		return -2;
	}
	(*clt->co) << ".\r\n";
	return 0;
}

#undef DD2INT

int ns_post(ClientData * clt, int argc, char *argv[])
{
	VERB(slog.p(Logger::Debug) << "NewsCache::ns_post\n");
	if (argc != 1) {
		(*clt->co) << "501 Syntax: post\r\n";
		return -1;
	}

	Article art;

	try {
		// request article to be posted
		(*clt->co) << "340 send article to be posted.\r\n";
		clt->co->flush();
		art.read(*clt->ci);

		// check whether client is authorized to post this article.
		string newsgroups = art.getfield("newsgroups:");
		unsigned int i1 = 0, i2 = newsgroups.find(",");
		for (;;) {
			string group = newsgroups.substr(i1, i2);
			if (clt->access_entry->postTo.
			    matches(group.c_str()) <= 0) {
				(*clt->
				 co) << "480 authentication required\r\n";
				return 0;
			}
			if (i2 == string::npos)
				break;
			i1 = i2 + 1;
			i2 = newsgroups.find(",", i1);
		}
	}
	catch(const InvalidArticleError & iae) {
		(*clt->co) << "441 invalid article\r\n";
		return -1;
	}
	catch(const NoSuchFieldError & nsfe) {
		(*clt->co) << "441 invalid article\r\n";
		return -1;
	}

	try {
		// client is authorized to post the article
		clt->srvr->post(&art);
		(*clt->co) << "240 Article posted\r\n";
		return 0;
	}
	catch(const InvalidArticleError & iae) {
		(*clt->co) << "441 invalid article\r\n";
		return -1;
	}
	catch(const NotAllowedError & nae) {
		(*clt->co) << "440 posting not allowed\r\n";
		return -1;
	}
	catch(const Error & e) {
		(*clt->co) << "449 operation failed\r\n";
		return -1;
	}
}

int ns_quit(ClientData * clt, int argc, char *argv[])
{
	(*clt->co) << "205 Good bye\r\n";
	return 1;
}

int ns_xmotd(ClientData * clt, int argc, char *argv[])
{
	(*clt->co) << "500 not yet supported\r\n";
	return -1;
}


/*
 * 221 Header follows
 * 412 No news group current selected
 * 420 No current article selected
 * 430 no such article
 * 502 no permission
 */
int ns_xover(ClientData * clt, int argc, char *argv[])
{
	int i = 1, fst, lst;
	char *p;

	if (clt->groupname[0] == '\0') {
		(*clt->co) << "412 No newsgroup currently selected\r\n";
		return -1;
	}
	if (strcmp(argv[0], "xhdr") == 0) {
		i = 2;
	}
	switch (argc - i) {
	case 0:
		fst = lst = clt->nbr;
		break;
	case 1:
		p = argv[i];
		if (isdigit(*p))
			fst = lst = strtol(argv[i], &p, 10);
		if ((*p) == '\0')
			break;
		if ((*p) == '-') {
			lst = UINT_MAX;
			p++;
			if (isdigit(*p))
				lst = strtol(p, &p, 10);
			if ((*p) == '\0')
				break;
		}
	default:
		(*clt->co) << "501 Syntax: xover [range]\r\n";
		return -1;
	}
	try {
		if (!clt->grp)
			clt->grp = clt->srvr->getgroup(clt->groupname);
	}
	catch(const NoSuchGroupError & nsge) {
		(*clt->co) << "411 no such news group\r\n";
		return -1;
	}
	catch(const Error & e) {
		e.print();
		(*clt->co) << "412 operation failed\r\n";
		return -1;
	}

	ASSERT2(clt->grp->testdb());
	switch (i) {
	case 1:
		(*clt->co) << "224 Overview information follows\r\n";
		clt->grp->printoverdb(*clt->co, fst, lst);
		break;
	case 2:
		(*clt->
		 co) << "221 " << p << "[" << fst << "-" << lst << "]\r\n";
		clt->grp->printheaderdb(*clt->co, argv[1], fst, lst);
		break;
	}
	(*clt->co) << ".\r\n";
	return 0;
}

int ns_xdebug(ClientData * clt, int argc, char *argv[])
{
	if (argc == 3) {
		if (strcmp(argv[1], "dump") == 0) {
			if (strcmp(argv[2], "authorization") == 0) {
				(*clt->
				 co) <<
			      "xxx authorization data follows\r\n";
				(*clt->co) << *clt->access_entry << "\r\n";
				(*clt->co) << ".\r\n";
			}
			if (strcmp(argv[2], "configuration") == 0) {
				(*clt->
				 co) <<
			      "xxx configuration data follows\r\n";
				Cfg.printParameters (clt->co);
				(*clt->co) << ".\r\n";
			}
		}
		return 0;
	}

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			(*clt->co) << "Valid commands:\r\n";
			(*clt->co) << "xdebug dump authorization:\r\n";
			(*clt->co) << "xdebug dump configuration:\r\n";
			(*clt->co) << ".\r\n";
			return 0;
		}
	}

	(*clt->co) << "501 Syntax: xdebug help\r\n";
	return -1;
}


#ifdef HAVE_LIBWRAP
#ifndef WITH_SYSLOG
/* This is from my syslog.h */
#define LOG_EMERG       0	/* system is unusable */
#define LOG_ALERT       1	/* action must be taken immediately */
#define LOG_CRIT        2	/* critical conditions */
#define LOG_ERR         3	/* error conditions */
#define LOG_WARNING     4	/* warning conditions */
#define LOG_NOTICE      5	/* normal but significant condition */
#define LOG_INFO        6	/* informational */
#define LOG_DEBUG       7	/* debug-level messages */
#endif

/* needed for TCP wrappers */
int allow_severity = LOG_INFO;
int deny_severity = LOG_NOTICE;
#endif
void set_client_command_table (ClientData &clt)
{
	clt.client_command_map.enableAll();
	if (!(clt.access_entry->access_flags & AccessEntry::af_read)) {
		clt.client_command_map.disableRead();
	}

	if (!(clt.access_entry->access_flags & AccessEntry::af_post)) {
		clt.client_command_map.disablePost ();
	}

	if (!(clt.access_entry->access_flags & AccessEntry::af_debug)) {
		clt.client_command_map.disableDebug ();
	}
}

/* nnrpd(fd)
 */
void nnrpd(int fd)
{
	ClientData clt;
	char req[1024], oreq[1024], *rp;
	char *argv[256];
	int argc;
	const char *cmd;
	map < string, nnrp_command_t * >::iterator cmdp, end =
	    nnrp_commands.end();
	int errc = 0;
	sockstream sock_stream;
	int nice;

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

	if (fd >= 0) {
		sock_stream.attach(fd);

		sock_stream.setkeepalive();
		sock_stream.setnodelay();

		clt.co = &sock_stream;
		clt.ci = &sock_stream;

		// get network address and name of client
		clt.socklen = sizeof(clt.sock);
		if (getpeername
		    (fd, (struct sockaddr *) &clt.sock,
		     &clt.socklen) < 0) {
			// Cannot get socket --- connection from stdin?
			sprintf(clt.client_name, "fd#%d", fd);
			clt.client_logname = clt.client_name;
		} else {
			// get the name of the client and store it in clt.client_name.
			// if the client does not have a name, use the ip-address instead
			// store it in nntp_posting_host as used by libnserver
			clt.client_name[0] = '\0';
			getnameinfo((const struct sockaddr *) &clt.sock,
				    clt.socklen,
				    clt.client_name, sizeof(clt.client_name),
				    NULL, 0, 0);

			struct addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags =
			  (clt.sock.ss_family == AF_INET6) ? AI_V4MAPPED : 0;
			hints.ai_family = clt.sock.ss_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			struct addrinfo *res = NULL;
			getaddrinfo(clt.client_name, NULL, &hints, &res);

			// have to verify the address we got from getnameinfo
			bool found_addr = false;
			struct addrinfo *addr = res;
			while (addr != NULL) {
				if (addr->ai_addr->sa_family == clt.sock.ss_family) {
					if (addr->ai_addr->sa_family == AF_INET) {
						if (((const struct sockaddr_in *) addr->ai_addr)->sin_addr.s_addr ==
						    ((const struct sockaddr_in *) &clt.sock)->sin_addr.s_addr) {
							found_addr = true;
							break;
						}
					} else if (addr->ai_addr->sa_family == AF_INET6) {
						if (!memcmp(((const struct sockaddr_in6 *) addr->ai_addr)->sin6_addr.s6_addr,
							    ((const struct sockaddr_in6 *) &clt.sock)->sin6_addr.s6_addr,
							    16)) {
							found_addr = true;
							break;
						}
					}
				}

				addr = addr->ai_next;
			}
			freeaddrinfo(res);

			if (!found_addr) {
				clt.client_name[0] = '\0';
			}

			clt.client_addr[0] = '\0';
			getnameinfo((const struct sockaddr *) &clt.sock,
				    clt.socklen,
				    clt.client_addr, sizeof(clt.client_addr),
				    NULL, 0, NI_NUMERICHOST);

			if (clt.client_name[0] == '\0') {
				strcpy(clt.client_name, clt.client_addr);
			}
			strcpy(nntp_posting_host, clt.client_name);

			// add the client's name to its logging name (clt.client_logname),
			// if names should be logged
			if (Cfg.LogStyle & Config::LogName) {
				clt.client_logname = clt.client_name;
			}

			if (Cfg.LogStyle & Config::LogAddr) {
				if (clt.client_logname.length() > 0)
					clt.client_logname += " [";
				else
					clt.client_logname += '[';
				clt.client_logname += clt.client_addr;
				clt.client_logname += ']';
			}
			// check whether the client is allowed access according to
			// libwrap
#ifdef HAVE_LIBWRAP
			// Check the hosts_access configuration; emulate INN error message
			if (!hosts_ctl(PACKAGE,
				       clt.client_name, clt.client_addr,
				       STRING_UNKNOWN)) {
				slog.p(Logger::Notice) << clt.
				    client_logname << " denied - hosts.allow\n";
				(*clt.
				 co) <<
			       "502 You have no permission to talk.  Goodbye.\n";
				clt.co->flush ();
				return;
			}
#endif
		}
	} else {
		cin.unsetf(ios::skipws);
		clt.ci = &cin;
		clt.co = &cout;
		strcpy(clt.client_name, "stdin");
		clt.client_logname = clt.client_name;
		fd = 0;
	}

#ifdef NO_STREAM_BUFFERING
	{
		streambuf *b;
		b = clt.ci->rdbuf();
		if (b)
			b->setbuf(0, 0);
		b = clt.co->rdbuf();
		if (b)
			b->setbuf(0, 0);
	}
#endif

	// check whether the client is allowed access according to
	// our own access configuration
	clt.access_entry =
		Cfg.client(clt.client_name,
			   (struct sockaddr *) &clt.sock, clt.socklen);
	if (!clt.access_entry || !clt.access_entry->access_flags) {
//   nnrpd_deny:
		slog.p(Logger::Notice) << clt.
		    client_logname << " denied\n";
		(*clt.
		 co) << "502 You have no permission to talk.  Goodbye.\n";
		clt.co->flush();
		return;
	} else {
		slog.p(Logger::Debug) << "nnrpd: access_entry name matched: "
			<< clt.access_entry->hostname << "\n";
	}

	slog.p(Logger::Notice) << clt.client_logname << " connect\n";

	if (clt.access_entry->authentication.getType() != "none") {
		clt.auth_state = AUTH_REQUIRED;
	}
	if (clt.access_entry->authentication.getType() == "unix" &&
	    clt.access_entry->authentication.getNrOfFields() > 1) {
		// authentication optional, not required
		clt.auth_state = AUTH_NOT_REQUIRED;
	}
	if (clt.access_entry->authentication.getType() == "file") {
		// authentication optional, not required
		clt.auth_state = AUTH_NOT_REQUIRED;
	}
	if (clt.access_entry->authentication.getType() == "pam") {
		// authentication optional, not required
		clt.auth_state = AUTH_NOT_REQUIRED;
	}
	if (clt.access_entry->authentication.getType() == "pam+file") {
		// authentication optional, not required
		clt.auth_state = AUTH_NOT_REQUIRED;
	}

	// Set client command table, can be changed after successfully
	// authentication!
	set_client_command_table (clt);

	try {
		clt.srvr = new CServer(Cfg.SpoolDirectory, &(Cfg.srvrs));
		clt.srvr->setttl(Cfg.ttl_list, Cfg.ttl_desc);
	}
	catch(const SystemError & se) {
		slog.
		    p(Logger::
		      Alert) << "CServer failed, check permissions\n";
		se.print();
		(*clt.
		 co) << "400 " PACKAGE " " VERSION
	       ", service not available\r\n";
		exit(1);
	}

	(*clt.
	 co) << "200 " PACKAGE " " VERSION ", accepting NNRP commands\r\n";
	do {
		flush(*clt.co);

		alarmed = 0;
		alarm (Cfg.ClientTimeout);
		clt.ci->getline(req, sizeof(req), '\n');
		alarm (0);
		if (alarmed) {
			alarmed = 0;
			(*clt.co) << "400 " PACKAGE " " VERSION ", service timed out!\r\n";
			slog.p(Logger::Notice) << clt.client_logname << " ClientTimeout reached\n";
			goto client_exit;
		}
		if (Xsignal != -1) {
			(*clt.co) << "400 " PACKAGE " " VERSION ", service discontinued\r\n";
			slog.p(Logger::Notice) << clt.client_logname << " discontinued due to Signal\n";
			goto client_exit;
		}
		if (clt.ci->eof()) {
			slog.p(Logger::Debug) << "NewsCache.cc input stream eof!\n";
			break;
		}
		if (!clt.ci->good()) {
			slog.p(Logger::Debug) << "NewsCache.cc input stream not good!\n";
			break;
		}
		rp = req + strlen(req);
		while (rp > req) {
			rp--;
			if (!isspace(*rp))
				break;
			else
				*rp = '\0';
		}
		strcpy(oreq, req);
		// FIXME: better solution?
		if (strncasecmp(oreq, "authinfo", 8))
			slog.p(Logger::Info) << clt.
			    client_logname << " " << oreq << "\n";
		else
			slog.p(Logger::Info) << clt.
			    client_logname << " authinfo\n";

		// Split command into arguments
		for (rp = req, argc = 0; *rp && argc < 256; rp++) {
			if (isspace(*rp)) {
				*rp = '\0';
			} else {
				if (rp == req || *(rp - 1) == '\0')
					argv[argc++] = rp;
				if (argc == 1)
					*rp = tolower(*rp);
			}
		}
		if (!argc)
			continue;
		if (argc == 256) {
			(*clt.co) << "500 Line too long\r\n";
			continue;
		}
		// Call function for command
		cmd = argv[0];
		if (clt.auth_state == AUTH_REQUIRED &&
		    strcasecmp(cmd, "authinfo") != 0 &&
		    strcasecmp(cmd, "quit") != 0 &&
		    strcasecmp(cmd, "help") != 0) {
			(*clt.
			 co) << "480 Authentication required (" << cmd <<
		       ")\r\n";
			continue;
		}

		cmdp = clt.client_command_map.find(argv[0]);
		if (cmdp == clt.client_command_map.end() || !cmdp->second->func) {
			slog.p(Logger::Notice) << clt.client_logname
			    << " unrecognized " << oreq << "\n";
			(*clt.co) << "500 What?\r\n";
			continue;
		}

		errc = cmdp->second->func(&clt, argc, argv);
		if (!(Cfg.LogStyle & Config::LogINN)) {
			if (errc < 0) {
				if (strncasecmp(oreq, "authinfo", 8))
					slog.p(Logger::Notice) << clt.
				    		client_logname << " failed " << oreq <<
				    		"\n";
				else 
					slog.p(Logger::Notice) << clt.
						client_logname << " failed authinfo for user  " <<
						clt.auth_user << "\n";
			}
		if (clt.auth_failures >= clt.auth_max_failures) {

			(*clt.  co) << "400 " PACKAGE " " VERSION
		       ", service discontinued (max passwords retries)\r\n";
			goto client_exit;
		}
		}
	} while (errc == 0 || errc == -1);

      client_exit:
	if (clt.stat_artingrp) {
		slog.p(Logger::Notice) << clt.client_logname
		    << " group " << clt.groupname
		    << " " << clt.stat_artingrp << "\n";
		clt.stat_artingrp = 0;
	}
	slog.p(Logger::Notice) << clt.client_logname
	    << " exit articles " << clt.stat_articles
	    << " groups " << clt.stat_groups << "\n";
	clt.co->flush();
	delete clt.srvr;
}

void nntpd()
{
	slog.p(Logger::Notice) << "NewsCache Server Start\n";

	/* set nice value for master server */
#ifdef HAVE_SETPRIORITY
	errno = 0;
	int nice = Cfg.NiceServer;
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

	/* nobody is connecting to NewsCache currently */
	nntp_connections = 0;

	/* create socket and set some socket options */
	string hostname;
	const char *nodename = NULL;
	const char *servname = NULL;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE | AI_NUMERICHOST;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* fill the sockaddr with the port we should listen too */
	if (Cfg.CachePort[0] != '\0') {
		// old variant
		if (Cfg.CachePort[0] != '#') {
			servname = Cfg.CachePort + 1;
			hints.ai_flags |= AI_NUMERICSERV;
		} else {
			servname = Cfg.CachePort;
		}
	} else {
		if (Cfg.ListenTo[0] == '[') {
			const char *end = strrchr(Cfg.ListenTo, ']');
			if ((end == NULL) ||
			    ((end[1] != '\0') && (end[1] != ':'))) {
				slog.
				  p(Logger::
				    Error) << cmnd <<
				  ": Invalid parameter ListenTo: " <<
				  Cfg.ListenTo << "\n";
				exit(1);
			}

			hostname.assign(const_cast<const char *>(Cfg.ListenTo + 1), end);
			nodename = hostname.c_str();

			if (end[1] == ':') {
				servname = end + 2;
			} else {
				servname = "nntp";
			}
		} else {
			const char *colon = strrchr(Cfg.ListenTo, ':');
			if (colon != NULL) {
				hostname.assign(const_cast<const char *>(Cfg.ListenTo), colon);
				servname = colon + 1;
			} else {
				hostname.assign(Cfg.ListenTo);
				servname = "nntp";
			}

			if (hostname != "DEFAULT") {
				nodename = hostname.c_str();
			}
		}
	}

	struct addrinfo *res;
	if (getaddrinfo(nodename, servname, &hints, &res)) {
		slog.
		  p(Logger::
		    Error) << cmnd <<
		  ": Can't resolve parameter ListenTo: " << Cfg.
		  ListenTo << "\n";
		exit(1);
	}

	int sock = -1;
	struct addrinfo *addr = res;
	while ((sock < 0) && (addr != NULL)) {
		if ((sock = socket(addr->ai_addr->sa_family,
				   SOCK_STREAM, 0)) < 0) {
			slog.
			    p(Logger::
			      Error) << "socket failed: " <<
			    strerror(errno) << "\n";
			exit(1);
		}

		int one = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
			       (char *) &one, sizeof(int)) < 0) {
			slog.
			    p(Logger::
			      Error) << "setsockopt failed: " <<
			    strerror(errno) << "\n";
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       (char *) &one, sizeof(int)) < 0) {
			slog.
			    p(Logger::
			      Error) << "setsockopt failed: " <<
			    strerror(errno) << "\n";
		}

		if (bind(sock, addr->ai_addr, addr->ai_addrlen) < 0) {
			slog.
			  p(Logger::
			    Error) << "can't bind socket: " << strerror(errno) <<
			  "\n";
			exit(1);
		}

		addr = addr->ai_next;
	}			/* end create socket and set some socket options */
	freeaddrinfo(res);

	{			/* store pid in Cfg.PidFile */
		ofstream pid(Cfg.PidFile);
		pid << getpid() << endl;
		if (!pid.good()) {
			slog.
			    p(Logger::Warning) << "cannot open pid file\n";
		}
	}

	setugid(Cfg.Username, Cfg.Groupname);
	listen(sock, 4);
	for (;;) {
		int clt_fd;
		int clt_pid = 0;

		/* Accept connection */
		do {
			if (reReadConfig) {
				Cfg.init();
				Cfg.read(config_file);
				reReadConfig = false;
			}

			errno = 0;
			clt_fd = accept(sock, NULL, NULL);
			if (Xsignal != -1) {
				close(sock);
				exit(0);
			}
		} while (errno == EINTR);
		if (clt_fd < 0) {
			slog.
			    p(Logger::
			      Warning) << "accept failed: " <<
			    strerror(errno) << "\n";
			continue;
		}
#ifdef CONF_MultiClient
		if (nntp_connections >= Cfg.MaxConnections
		    || (clt_pid = fork()) < 0) {
			if (clt_pid < 0) {
				slog.
				    p(Logger::
				      Error) << "fork failed: " <<
				    strerror(errno) << "\n";
			}
			write(clt_fd, "400 too many users\r\n", 21);
			close(clt_fd);
		} else {
			// success
			if (clt_pid == 0) {
				// child
				close(sock);

				try {
					nnrpd(clt_fd);
				} catch (const UsageError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "UsageError ";
					e.print();
				} catch (const NotAllowedError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NotAllowdError ";
					e.print();
				} catch (const NoSuchArticleError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NoSuchArticleError ";
					e.print();
				} catch (const DuplicateArticleError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "DuplicateArticleError ";
					e.print();
				} catch (const NoSuchGroupError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NoSuchGroupError ";
					e.print();
				} catch (const NoNewsServerError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NoNewsServerError ";
					e.print();
				} catch (const NoSuchFieldError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NoSuchFieldError ";
					e.print();
				} catch (const NSError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "NSError ";
					e.print();
				} catch (const AssertionError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "AssertionError ";
					e.print();
				} catch (const IOError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "IOError ";
					e.print();
				} catch (const SystemError &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "SystemError ";
					e.print();
				} catch (const Error &e) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "Error ";
					e.print();
				} catch ( ... ) {
					slog.p(Logger::Error) << "nnrpd caught "
						<< "unknown!!"	<< "\n";
				}
				close(clt_fd);
				exit(0);
			}
			//Parent

			while ((close(clt_fd) < 0) && (errno == EINTR)) {
			}
			nntp_connections++;
		}
#else
		nnrpd(clt_fd);
		close(clt_fd);
#endif
	}

	close(sock);
}

void sigchld(int num)
{
	int pid;
	int st;

	//slog.p(Logger::Debug) << "receiving signal SIGCHLD: " << num << "\n";
	/* Reinstall the signal handler */
#ifdef HAVE_SIGACTION
	struct sigaction action;
	action.sa_handler = sigchld;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(num, &action, NULL);
#else
	signal(num, sigchld);
#endif

	while ((pid = waitpid(0, &st, WNOHANG)) > 0) {
		if (WIFEXITED(st) || WIFSIGNALED(st)) {
			// child terminated
			nntp_connections--;
			if (st) {
				if (WIFEXITED(st)) {
					slog.
					    p(Logger::
					      Error) << pid << " returned "
					    << WEXITSTATUS(st) << "\n";
				} else if (WIFSIGNALED(st)) {
					slog.
					    p(Logger::
					      Error) << pid <<
					    " caught signal " <<
					    WTERMSIG(st) << "\n";
				}
			}
		}
	}
}

void catchsigalarm(int num)
{
	//slog.p(Logger::Debug) << "receiving signal SIGALRM: " << num << "\n";
	alarmed=1;
#ifdef HAVE_SIGACTION
	/* Reinstall the signal handler */
	struct sigaction action;
	action.sa_handler = catchsigalarm;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(num, &action, NULL);
#else
	signal(num, catchsigalarm);
#endif
}

void catchsighup(int num)
{
	//slog.p(Logger::Debug) << "receiving signal SIGHUP: " << num << "\n";
	reReadConfig=true;
#ifdef HAVE_SIGACTION
	/* Reinstall the signal handler */
	struct sigaction action;
	action.sa_handler = catchsighup;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(num, &action, NULL);
#else
	signal(num, catchsighup);
#endif
}

void catchsignal(int num)
{
	//slog.p(Logger::Debug) << "receiving signal: " << num << "\n";
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

#define USAGE \
"       -h, --help\n"\
"              Show summary of options.\n"\
"\n"\
"       -v, --version\n"\
"              Show version of program.\n"\
"\n"\
"       -f --fqdn\n"\
"              Print what newscache thinks is the fully qualified domain  name.\n"\
"\n"\
"       -c --configuration config-file\n"\
"\n"\
"       -i --inetd\n"\
"              newscache is used with inetd and read from stdin.\n"\
"\n"\
"       -d --debug\n"\
"              Do not detach.\n"\
"\n"\
"       -p --print-parameter\n"\
"              Print current parameter settings.\n"\
"\n"\
"       -o --configurtion-options\n"\
"              Print all options specified in the configure phase.\n"

int main(int argc, char **argv)
{
#ifndef WITH_SYSLOG
	char logfile[MAXPATHLEN];
	time_t t;
	pid_t p;
#endif
	int opt_config = 0, opt_inetd = 0, opt_debug = 0, opt_print_para = 0;
	int opt_config_options = 0;
	int aerr = 0, c;

	sprintf(config_file, "%s/newscache.conf", SYSCONFDIR);

	cmnd = argv[0];
	while (1) {
#ifdef HAVE_GETOPT_H
		int option_index = 0;
		static struct option long_options[] = {
			{"version", 0, 0, 'v'},
			{"fqdn", 0, 0, 'f'},
			{"help", 0, 0, 'h'},
			{"configuration", 1, 0, 'c'},
			{"inetd", 0, 0, 'i'},
			{"debug", 0, 0, 'd'},
			{"print-parameter", 0, 0, 'p'},
			{"configuration-options", 0, 0, 'o'},
			{0, 0, 0, 0}
		};

		c = getopt_long (argc, argv, "vfhc:idpo", long_options,
				&option_index);
#else
		c = getopt (argc, argv, "vfhc:idpo");
#endif
		if (c == -1) 
			break;

		switch (c) {
			case 'v':
				cout << PACKAGE << " " VERSION << endl;
				exit (0);
				break;
			
			case 'f':
				cout << getfqdn () << endl;
				exit (0);
				break;
			
			case 'h':
				cout << "Usage: " << cmnd << " [options]\n"
					<< USAGE;
				exit (0);
				break;
			
			case 'c':
				strcpy (config_file, optarg);
				++opt_config;
				break;

			case 'i':
				++opt_inetd;
				break;

			case 'd':
				++opt_debug;
				break;
			case 'p':
				++opt_print_para;
				break;
			case 'o':
				++opt_config_options;
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
		Cfg.read(config_file);
		strcpy(nntp_hostname, Cfg.Hostname);
	}
	catch(const IOError & io) {
		cerr << "unexpected EOF in " << config_file << "\n";
		exit(2);
	}
	catch(const SyntaxError & se) {
		cerr << se._errtext << "\n";
		exit(2);
	}

	if (opt_print_para) {
		cout << "# This output is generated with newscache -p" << endl;
		Cfg.printParameters (&cout);
		exit (0);
	}

	if (opt_config_options) {
		for (int i=0; ConfigurationOptions[i] != NULL; i++) {
			cout << ConfigurationOptions[i] << " ";
		}
		cout << endl;
		exit (0);
	}

#ifdef WITH_SYSLOG
	slog.open(PACKAGE, LOG_NDELAY | LOG_PID, LOG_NEWS);
#else
	sprintf (logfile, "%s/newscache.log", Cfg.LogDirectory);
	slog.open (logfile);
#endif

	// signal handling
	sockstream::alarm_indicator(alarmed);
	Xsignal = -1;
	reReadConfig = false;

#ifdef HAVE_SIGACTION
	struct sigaction action;

	// alarm - client timeout signal
	action.sa_handler = catchsigalarm;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);

	// reread config file
	action.sa_handler = catchsighup;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGHUP, &action, NULL);

	// signals indicating to terminate NewsCache
	action.sa_handler = catchsignal;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGPIPE, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	// ignored signals
	action.sa_handler = SIG_IGN;
	sigaction(SIGUSR1, &action, NULL);
	sigaction(SIGUSR2, &action, NULL);

	// clean zombies
	action.sa_handler = sigchld;
	sigaction(SIGCHLD, &action, NULL);
#else
	signal(SIGALRM, catchsigalarm);
	signal(SIGHUP, catchsighup);
	signal(SIGINT, catchsignal);
	signal(SIGPIPE, catchsignal);
	signal(SIGTERM, catchsignal);

	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	signal(SIGCHLD, sigchld);
#endif

	if (Cfg.ServerType == Config::inetd)
		opt_inetd = 1;

	if (opt_inetd) {
		if (getuid() == CONF_UIDROOT)
			setugid(Cfg.Username, Cfg.Groupname);
		nnrpd(-1);
	} else {
		if (opt_debug == 0) {
			switch (fork()) {
			case -1:
				cerr << "cannot fork\n";
				slog.p(Logger::Error) << "cannot fork\n";
				break;
			case 0:
				if (setsid() == -1) {
					cerr << "setsid failed\n";
					slog.
					    p(Logger::
					      Error) << "setsid failed\n";
					exit(1);
				}
				close(STDIN_FILENO);
				close(STDOUT_FILENO);
				close(STDERR_FILENO);
				break;
			default:
				return 0;
			}
		}
		nntpd();
	}

	return 0;
}
