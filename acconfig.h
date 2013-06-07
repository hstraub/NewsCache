/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* These get defined to the options given to configure */
#undef HAVE_LIBWRAP
#undef HAVE_SSTREAM
#undef MD5_CRYPT
#undef MD5_AUTO
#undef SHADOW_PASSWORD
#undef PAM_AUTH
#undef PAM_DEFAULT_SERVICENAME
#undef WITH_UNIQUE_PACKAGE_NAME
#undef WITH_LIBGPP
#undef WITH_SYSLOG
#undef WITH_EXCEPTIONS
#undef SYSCONFDIR
#ifdef WITH_EXCEPTIONS
#define EXCEPTION(x) throw x
#else
#define EXCEPTION(x)
#endif
#undef ENABLE_HEADERLOG
#undef ENABLE_DEBUG
#undef ENABLE_ASSERTIONS
#undef PLAIN_TEXT_PASSWORDS

/* Which type does the third argument of accept have */
#undef SOCKLEN_TYPE

/* These get defined to the package/version numbers in configure.in */
#undef PACKAGE
#undef VERSION

/* Other obsolete configuration parameters */
#undef CONF_ServerRoot
#undef CONF_UIDROOT

/* Package names for NewsCacheClean and updatenews */
#undef PACKAGE_NEWSCACHECLEAN
#undef PACKAGE_UPDATENEWS


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
