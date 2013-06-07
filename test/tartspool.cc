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

#include "Article.h"
#include "ArtSpooler.h"
#include "Logger.h"

Logger slog;

using namespace std;

int main(int argc, char **argv)
{
	try {
		string spoolDir("test");
		ifstream is;
		ArtSpooler spooler(spoolDir);
		Article a, *pA;

		if (argc == 2) {
			is.open(*(argv + 1));
			a.read(is);
			is.close();
			spooler.spoolArt(a);
		} else {
			pA = spooler.getSpooledArt();
			if (pA != NULL) {
				spooler.storeBadArt(*pA);
				delete pA;
			}
		}

		return (0);
	}
	catch(SystemError e) {
		e.print();
		return (1);
	}
	catch(Error e) {
		e.print();
		cerr << "Duplicate article" << endl;
		return (1);
	}
	catch(...) {
		cerr << "undefined error" << endl;
		return (1);
	}
}
