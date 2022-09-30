#ifndef __ArtSpooler_h__
#define __ArtSpooler_h__

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

#include "Article.h"
#include "ObjLock.h"

/** 
 * \class ArtSpooler
 * \author Herbert Straub
 * \date 2003
 * \brief ArtSpooler manages the Spool Directory.
 *
 * ArtSpooler manages the Spool Directory with the Articles. It uses the
 * Parameter spoolDir to determine the Path to the Spool. ArtSpooler creates
 * the following subdirectories:
 *   \li .artSpool (Directory)
 *   \li .badArcticles (Directory),
 *   \li .resourceSpooler.lock (LockFile).
 */
class ArtSpooler {
  public:
	/**
	 * If the spool directory doesn't exists, the contstructor creates
	 * it. It uses ObjLock to manage the access to the Spool.
	 * \param spoolDir Spool Directory
	 * \throw Error
	 * \todo Das ist ein Testpunkt
	 */
	ArtSpooler(const std::string & spoolDir);

	~ArtSpooler();

	/**
	 * Add this article to the spool. The naming convention for
	 * articles in the spool: art_nr.txt. nr is a number.
	 * A shared Lock is used.
	 * \param a Article
	 * \throw Error
	 */
	void spoolArt(Article & a);

	/**
	 * Get one article from the Spool (don't forget delete). 
	 * It returns NULL, if no article is
	 * available. A exclusive lock is used.
	 *  \return A pointer to the article or NULL.
	 */
	Article *getSpooledArt(void);


	/**
	 * Store the article in the "Bad Article Store".
	 * Bad articles (ex. POST error) can be stored with this method.
	 * A operator intervention is required to manage this store.
	 * \param a Article
	 */
	void storeBadArt(Article & a);

      private:

	/**
	 * Create a unique ID
	 * \return The ID String
	 */
	std::string createID(void);

	/**
	 * Extract ID from article message id
	 * \return The ID String
	 */
	std::string extractID(Article & a);

	/**
	 * store Article in path
	 * return 0 success, 1 duplicated
	 */
	int storeArticle(const std::string & path, Article & a);

  private:
	std::string spoolDir;
	std::string artSpool;
	std::string badArticles;
	ObjLock *pLock;
};


#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
