#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include "config.h"
#include "Debug.h"
#include "Lexer.h"
#include "Config.h"
#include "NServer.h"

using namespace std;

/****************************************************************
 * CONFIG
 ****************************************************************/
#define STRNCPY(dest,source) \
    if(source.length()>=sizeof(dest))\
      throw SyntaxError(lex,"argument too long", ERROR_LOCATION);\
    strcpy(dest,source.c_str());

#define HANDLESTRGARG(name) \
    if(tok==#name) {\
      a1=lex.getToken();\
      STRNCPY(name,a1);\
    }

#define HANDLEINTARG(name) \
    if(tok==#name) {\
      a1=lex.getToken();\
      name=atoi(a1.c_str());\
    }


Config::Config()
{
	init();
}

Config::Config(const char *fn)
{
	init();
	read(fn);
}

void Config::init(void)
{

	Username[0] = Groupname[0] = '\0';
	strcpy(Admin, "notset");

	ServerType = standalone;
	strcpy(Hostname, getfqdn());
	CachePort[0] = '\0';
	strcpy (ListenTo, "DEFAULT");

	LogDirectory[0] = '\0';
	LogStyle = LogName + LogAddr;
	strcpy(SpoolDirectory, "/var/cache/newscache");
	SpoolSize = 1024 * 1024;
	PrefetchFile[0] = PidFile[0] = '\0';

	ClientTimeout = 1200;
	MaxConnections = 32;

	NiceServer = 0;
	NiceClient = 10;
	NiceClean = 3;

	ttl_list = 3600;	/* 1hr */
	ttl_desc = 3600;	/* 1hr */

	clnts.init();
	srvrs.init();

}

MPListEntry *Config::server(const char *group)
{
	return srvrs.server(group);
}

AccessEntry *Config::client(const char *name, struct in_addr addr)
{
	return clnts.client(name, addr);
}

void Config::read(const char *fn)
{
	Lexer lex(fn);
	string tok, a1, a2, a3;

	for (;;) {
		tok = lex.getToken();
		if (lex.eof())
			break;

		HANDLESTRGARG(SpoolDirectory)
		    else
			HANDLESTRGARG(LogDirectory)
			    else
			HANDLESTRGARG(PrefetchFile)
			    else
			HANDLESTRGARG(Username)
			    else
			HANDLESTRGARG(Groupname)
			    else
			HANDLESTRGARG(CachePort)
			    else
			HANDLESTRGARG(ListenTo)
			    else
			HANDLESTRGARG(Admin)
			    else
			HANDLESTRGARG(PidFile)
			    else
			HANDLESTRGARG(Hostname)
			    else
			HANDLEINTARG(SpoolSize)
			    else
			HANDLEINTARG(ClientTimeout)
			    else
			HANDLEINTARG(MaxConnections)
			    else
			HANDLEINTARG(NiceServer)
			    else
			HANDLEINTARG(NiceClient)
			    else
			HANDLEINTARG(NiceClean)
			    else
		if (tok == "LogStyle") {
			LogStyle = 0;
			for (;;) {
				a1 = lex.getToken();
				if (a1 == "strict-inn")
					LogStyle |= LogINN;
				else if (a1 == "hostname")
					LogStyle |= LogName;
				else if (a1 == "ip-address")
					LogStyle |= LogAddr;
				else
					break;
			}
			lex.putbackToken(a1);
		} else if (tok == "ServerType") {
			a1 = lex.getToken();
			if (a1 == "inetd")
				ServerType = inetd;
			else if (a1 == "standalone")
				ServerType = standalone;
			else
				throw SyntaxError(lex,
						  "illegal ServerType",
						  ERROR_LOCATION);
		} else if (tok == "ConfigVersion") {
			a1 = lex.getToken();
			if (atoi(a1.c_str()) != 5)
				throw SyntaxError(lex,
						  "expecting ConfigVersion 5",
						  ERROR_LOCATION);
		} else if (tok == "NewsServerList") {
			srvrs.read(lex);
		} else if (tok == "AccessList") {
			clnts.read(lex);
		} else if (tok == "Timeouts") {
			a1 = lex.getToken();
			a2 = lex.getToken();
			ttl_list = atoi(a1.c_str());
			ttl_desc = atoi(a2.c_str());
		} else {
			throw SyntaxError(lex, "unknown keyword",
					  ERROR_LOCATION);
		}
	}
}

void Config::printParameters (ostream *pOut)
{
	if (Username[0] != '\0') {
		*pOut << "Username " << Username << endl;
	}
	if (Groupname[0] != '\0') {
		*pOut << "Groupname " << Groupname << endl;
	}
	*pOut << "Admin " << Admin << endl;
	*pOut << "ServerType ";
	if (ServerType == inetd) {
		*pOut << "inetd" << endl;
	} else if (ServerType == standalone) {
		*pOut << "standalone" << endl;
	} else {
		*pOut << "UNDEFINED!" << endl;
	}
	if (CachePort[0] != '\0') {
		*pOut << "CachePort " << CachePort << endl;
	}
	*pOut << "ListenTo " << ListenTo << endl;
	if (LogDirectory[0] != '\0') {
		*pOut << "LogDirectory " << LogDirectory << endl;
	}
	*pOut << "SpoolDirectory " << SpoolDirectory << endl;
	*pOut << "LogStyle";
	if (LogStyle & LogINN) {
		*pOut << " strict-inn";
	}
	if (LogStyle & LogName) {
		*pOut << " hostname";
	}
	if (LogStyle & LogAddr) {
		*pOut << " ip-address";
	}
	*pOut << endl;
	*pOut << "SpoolSize " << SpoolSize << endl;
	if (PrefetchFile[0] != '\0') {
		*pOut << "PrefetchFile " << PrefetchFile << endl;
	}
	if (PidFile[0] != '\0') {
		*pOut << "PidFile " << PidFile << endl;
	}
	*pOut << "ClientTimeout " << ClientTimeout << endl;
	*pOut << "MaxConnections " << MaxConnections << endl;
	*pOut << "Timeouts " << ttl_list << " " << ttl_desc << endl;
	*pOut << "NiceServer " << NiceServer << endl;
	*pOut << "NiceClient " << NiceClient << endl;
	*pOut << "NiceClean " << NiceClean << endl;

	clnts.printParameters (pOut);
	srvrs.printParameters (pOut);
}
