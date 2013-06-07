## Process this file with automake to produce Makefile.in

## Global options
INCLUDES=-I@top_srcdir@/NVClasses-0.9.1 -I@top_srcdir@/NServer-0.9.1 -I@top_srcdir@/md5lib 

LDADD = ../NServer-0.9.1/libnserver.a ../NVClasses-0.9.1/libnvcontainer.a ../md5lib/libmd5.a @LDADD@

## Test Programs
noinst_PROGRAMS = tlock tartspool

#TESTS=tlock


tlock_SOURCES = tlock.cc
tartspool_SOURCES = tartspool.cc

