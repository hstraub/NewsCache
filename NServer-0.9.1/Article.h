#ifndef __Article_h__
#define __Article_h__

//#include <ctype.h>
//#include <stdio.h>

#include <string>

#include "NSError.h"
#include "Debug.h"
#include "readline.h"

/**
 * \class InvalidArticleError
 * \author Thomas Gschwind
 * Syntactic error in Article dectected
 */
class InvalidArticleError:public NSError {
      public:
	InvalidArticleError(const char *txt, const char *file,
			    const char *function, int line);
	
	InvalidArticleError(const std::string & txt, const char *file,
			      const char *function, int line);

	virtual void print(void);
};

/**
 * \class Article
 * \author Thomas Gschwind
 *
 * Handle a Article
 * \bug Method documentation is missing.
 */
class Article {
      private:

	/**
	 * Return the position of a given field in the article's header.
	 * \param ifld The name of the field to be searched for including `:'
	 * \throw InvalidArticleError If a syntactic error is detected.
	 */
	const char *find_field(const char *ifld) const;

      protected:
	int _nbr;
	std::string _text;
	const char *_ctext;

      public:
	enum {
		Head = 0x1,
		Body = 0x2
	};

	Article();
	Article(Article * a);
	Article(int artnbr);
	Article(int artnbr, const char *text, int textlen = 0);
	~Article();

	void read(std::istream & is);

	void setnbr(int nbr);
	
	void clear(void);
	
	void settext(const std::string & text);

	int getnbr(void) const;
	
	/* Return Text string
	 */
	std::string GetText(void);
	
	const char *c_str(void);
	
	int length(void) const;
	
	int has_field(const char *ifld);

	/*
	 * Return a header field of the article.
	 * \param ifld The name of the field to be extracted including `:'
	 * \throw NoSuchFieldError raised, if fld is not part of the
	 * 	overview database
	 */
	std::string getfield(const char *ifld, int Full = 0) const;

	void setfield(const char *ifld, const char *field_data);

	std::ostream & write(std::ostream & os, int flags = Head | Body);

	friend std::ostream & operator<<(std::ostream & os, Article & art);
};

#endif
