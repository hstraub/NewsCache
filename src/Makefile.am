## Process this file with automake to produce Makefile.in

## Global options
INCLUDES=-I@top_srcdir@/NVClasses-0.9.1 -I@top_srcdir@/NServer-0.9.1 -I@top_srcdir@/md5lib 

LDADD = ../NServer-0.9.1/libnserver.a ../NVClasses-0.9.1/libnvcontainer.a ../md5lib/libmd5.a @LDADD@

## Programs
EXTRA_PROGRAMS= cConfig crypt
bin_PROGRAMS  = updatenews
sbin_PROGRAMS = newscache newscacheclean

TESTS=cConfig

crypt_SOURCES = crypt.cc

cConfig_SOURCES = 	AccessList.cc\
	AccessList.h\
	cConfig.cc\
	Config.cc\
	Config.h

newscache_SOURCES = 	AccessList.cc\
	AccessList.h\
	NewsCache.cc\
	Config.cc\
	Config.h\
	setugid.cc\
	setugid.h

newscacheclean_SOURCES = 	AccessList.cc\
	AccessList.h\
	NewsCacheClean.cc\
	Config.cc\
	Config.h\
	setugid.cc\
	setugid.h

updatenews_SOURCES = 	AccessList.cc\
	AccessList.h\
	updatenews.cc\
	Config.cc\
	Config.h\
	setugid.cc\
	setugid.h

