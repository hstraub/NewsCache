## Process this file with automake to produce Makefile.in

## Global options
INCLUDES=-I@top_srcdir@/NVClasses-0.9.1 -I@top_srcdir@/NServer-0.9.1

## libraries used by the tests
LDADD=../libnserver.a @LDADD@ ../../NVClasses-0.9.1/libnvcontainer.a
# ../../md5lib/libmd5.a

## extra files needed for checking
EXTRA_DIST=article5283.in article5283.over.in article9621.in article9621.over.in article9622.in article9622.over.in overviewfmt1.in overviewfmt2.in overviewdb.in

EXTRA_PROGRAMS=cArticle cNSError cOverviewFmt cNewsgroupFilter

TESTS=cArticle cNSError cOverviewFmt cNewsgroupFilter

cArticle_SOURCES = cArticle.cc

cNSError_SOURCES = cNSError.cc

cOverviewFmt_SOURCES = cOverviewFmt.cc

cNewsgroupFilter_SOURCES = cNewsgroupFilter.cc