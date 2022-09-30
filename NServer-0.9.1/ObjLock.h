#ifndef __ObjLock_h__
#define __ObjLock_h__
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
#include <string>
#ifndef USE_FLOCK
#include <fcntl.h>
#endif

/** 
 * \class ObjLock
 * \author Herbert Straub
 * \date 2003
 * \brief Manage locks to a resource (file).
 * Manage locks to a resource (file). You can use a shared or a exclusive
 * lock in blocked or non-blocked operation.
 */
class ObjLock {
  public:
	//! Constructor
	//! \param name of Resource
	//! \throw SystemError Error creating Lock File
	ObjLock(const std::string & name);

	//! Destructor
	~ObjLock();

	enum { locked, not_locked };

	//! Lock Shared Block
	//! \throw SystemError Error in flock or fcntl
	void lockShBlk(void);

	//! Lock Shared Noblocked
	//! \returns locked or not_locked
	//! \throw SystemError Error in flock or fcntl
	int lockShNoBlk(void);

	//! Lock Exclusive Blocked
	//! \throw SystemError Error in flock or fcntl
	void lockExBlk(void);

	//! Lock Exclusive Noblocked
	//! \returns locked of not_locked
	//! \throw SystemError Error in flock or fcntl
	int lockExNoBlk(void);

	//! Unlock
	//! \throw SystemError Error in flock or fcntl
	void unlock(void);

  private:
	std::string name;
	int fd;
#ifndef USE_FLOCK
	struct flock l;
#endif

};

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
