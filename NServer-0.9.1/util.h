#ifndef _NSutil_h_
#define _NSutil_h_

extern int mkpdir(const char *dirname, int mode);
extern const char *group2dir(const char *name);

/**
 * \author Thomas Gschwind
 * Checks whether a newsgroup matches a given list of patterns. 
 * The pattern-list is a comma separated list of newsgroup-patterns 
 * that may contain a wildcard (*) at the end and start with a !
 * to indicate that the newsgroup must not match a pattern.
 * Eg: comp.*,!comp.os.win* matches all newsgroups starting
 * with comp., except those starting with comp.os.win. This
 * rule is especially designed for our special windows-friends ;)
 *
 * \param pattern The pattern-list.
 * \param group The name of the newsgroup terminated by \\0 or a
 * 	whitespace.
 * \return 
 * 	\li If the newsgroup is matched by a positive pattern, the number of
 * 		matching characters plus one will be returned.
 * 	\li If the newsgroup is matched by a negative pattern (one that
 * 		starts with !), minus the number of characters minus one
 * 		will be returned.
 */
extern int matchgroup(const char *pattern, const char *group);
extern int matchaddress(const char *pattern, const char *address);

/**
 * \author Thomas Gschwind
 * Get the fqdn of the local host. The primary host name may be
 * required for expansion of spool_directory and log_file_path, so
 * make sure it is set asap. It is obtained from uname(), but if
 * that yields an unqualified value, make a FQDN by using
 * gethostbyname to canonize it. Some people like upper case letters
 * in their host names, so we don't force the case.
 *
 * \return The FQDN
 */
extern const char *getfqdn(void);

#endif
