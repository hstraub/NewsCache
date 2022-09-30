#include <errno.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "setugid.h"

using namespace std;

void setugid(const char *uname, const char *gname)
{
	struct passwd *pwd;
	struct group *grp;

	if (gname && *gname) {
		if ((grp = getgrnam(gname)) == NULL) {
			cerr << "Cannot get grent for " << gname
			    <<
			    "\nPlease set the Groupname in newscache.conf\n";
			exit(1);
		}
		if (setregid(grp->gr_gid, grp->gr_gid) < 0) {
			cerr << "Cannot set effective groupid to " << grp->
			    gr_gid << ": " << strerror(errno) << endl;
			exit(1);
		}
	}
	if (uname && *uname) {
		if ((pwd = getpwnam(uname)) == NULL) {
			cerr << "Cannot get pwent for " << uname
			    <<
			    "\nPlease set the Username in newscache.conf\n";
			exit(1);
		}
		if (setreuid(pwd->pw_uid, pwd->pw_uid) < 0) {
			cerr << "Cannot set effective userid to " << pwd->
			    pw_uid << ": " << strerror(errno) << endl;
			exit(1);
		}
	}
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
