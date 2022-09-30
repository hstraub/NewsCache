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
#include <iostream>

#include <stdio.h>
#include <unistd.h>

#include "Logger.h"
#include "ObjLock.h"

Logger slog;

using namespace std;

int main(int argc, char **argv)
{
	fork();
	try {
		class ObjLock ObjLock(string("lock_test.file"));

		cout << "Trying get shared lock" << endl;
		ObjLock.lockShBlk();
		cout << "Shared gelockt." << endl;

		sleep(10);

		cout << "Trying unlock..." << endl;
		ObjLock.unlock();
		cout << "unlocked" << endl;

		cout << "Trying get exclusive lock" << endl;
		if (ObjLock.lockExNoBlk() == ObjLock::not_locked) {
			cout << "get NO lock" << endl;
		} else {
			cout << "get lock" << endl;
			ObjLock.unlock();
		}

		cout << "Trying get exclusive lock" << endl;
		ObjLock.lockExBlk();
		cout << "exlusive gelockt." << endl;

		sleep(10);

		cout << "Trying unlock..." << endl;
		ObjLock.unlock();
		cout << "unlocked" << endl;
	}
	catch(...) {
		cout << "Error Handler" << endl;
		perror("ErrorText");
		exit(1);
	}

}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
