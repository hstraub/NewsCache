#ifndef __Config_h__
#define __Config_h__

/* Copyright Th Gschwind 1996, 1997 */
#include <sys/param.h>
#include <time.h>

#include <iostream>

#include "OverviewFmt.h"
/*#include "Logger.h"*/
#include "MPList.h"
#include "AccessList.h"
#include "util.h"

/**
 * \class Config
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class Config {
      public:
	char Username[256];
	char Groupname[256];
	char Admin[512];

	enum { inetd, standalone };
	int ServerType;
	char Hostname[MAXHOSTNAMELEN];
	char CachePort[256];
	char ListenTo[MAXHOSTNAMELEN];


	enum {
		LogINN = 0x1,
		LogName = 0x2,
		LogAddr = 0x4
	};
	char LogDirectory[MAXPATHLEN];
	int LogStyle;
	char SpoolDirectory[MAXPATHLEN];
	int SpoolSize;
	char PrefetchFile[MAXPATHLEN];
	char PidFile[MAXPATHLEN];

	int ClientTimeout;
	int MaxConnections;
	time_t ttl_list;
	time_t ttl_desc;

	// Nice values
	int NiceServer;
	int NiceClient;
	int NiceClean;

	AccessList clnts;
	MPList srvrs;

	Config();
	Config(const char *fn);

	void init(void);
	void read(const char *fn);
	MPListEntry *server(const char *group);
	AccessEntry *client(const char *name, struct in_addr addr);
	void printParameters(std::ostream * pOut);
};

#endif
