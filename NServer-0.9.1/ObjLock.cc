/* Copyright (C) 2003 Herbert Straub
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Error.h"
#include "ObjLock.h"

ObjLock::ObjLock(const std::string & name)
{
	this->name = name;

	fd = open(name.c_str(), O_RDWR | O_CREAT | O_NONBLOCK,
		  S_IWUSR | S_IRUSR);
	if (fd == -1)
		throw(SystemError("Error in ObjLock constructor (open)"),
		      errno);
#ifndef USE_FLOCK
	memset((void *) &l, 0, sizeof(struct flock));
	l.l_whence = SEEK_SET;
#endif
}

ObjLock::~ObjLock()
{
	unlock();
	close(fd);
}

void ObjLock::lockShBlk(void)
{
#ifdef USE_FLOCK
	if (flock(fd, LOCK_SH) == -1)
		throw(SystemError("Error flock", errno));
#else
	l.l_type = F_RDLCK;
	if (fcntl(fd, F_SETLKW, &l) == -1)
		throw(SystemError("Error fcntl", errno));
#endif
}

int ObjLock::lockShNoBlk(void)
{
	int s;

#ifdef USE_FLOCK
	s = flock(fd, LOCK_SH | LOCK_NB);
#else
	l.l_type = F_RDLCK;
	s = fcntl(fd, F_SETLK, &l);
#endif
	if (s == 0) {
		return locked;
	} else if (errno == EWOULDBLOCK) {
		return not_locked;
	} else {
		throw(SystemError("Error in lockShNoBlk", errno,
				  ERROR_LOCATION));
	}
}

void ObjLock::lockExBlk(void)
{
#ifdef USE_FLOCK
	if (flock(fd, LOCK_EX) == -1)
		throw(SystemError("Error in flock", errno,
				  ERROR_LOCATION));
#else
	l.l_type = F_WRLCK;
	if (fcntl(fd, F_SETLKW, &l) == -1)
		throw(SystemError("Error in fcntl", errno,
				  ERROR_LOCATION));
#endif
}

int ObjLock::lockExNoBlk(void)
{
	int s;

#ifdef USE_FLOCK
	s = flock(fd, LOCK_EX | LOCK_NB);
#else
	l.l_type = F_WRLCK;
	s = fcntl(fd, F_SETLK, &l);
#endif
	if (s == 0) {
		return locked;
	} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
		return not_locked;
	} else {
		throw(SystemError("Error in flock or fcntl", errno,
				  ERROR_LOCATION));
	}
}

void ObjLock::unlock(void)
{
#ifdef USE_FLOCK
	if (flock(fd, LOCK_UN) == -1)
		throw(SystemError("Error in flock", errno,
				  ERROR_LOCATION));
#else
	l.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLKW, &l) == -1)
		throw(SystemError("Error in fcntl", errno,
				  ERROR_LOCATION));
#endif
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
