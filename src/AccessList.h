#ifndef _AccessList_h_
#define _AccessList_h_
#include <ctype.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/param.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "config.h"
#include "Debug.h"
#include "Error.h"
//#include "OverviewFmt.h"
#include "NewsgroupFilter.h"
#include "Lexer.h"



/*\class Authentication
 * \author Herbert Straub
 * \date 2003
 * \brief Authentication manages the parameter Authentication
 *
 * Authentication manages the values of the parameter Authentication.
 * The Authentication parameter ist a string with fields, seperated by
 * ":". The field[0] is the AuthenticationType (none, unix, file, ...).
 * All other fields are AuthenticationType specific.
 */
class Authentication {
  public:
	
	/*
	 * The default value for AuthenticationType is none.
	 */
	Authentication () : defAuth("none"){};

	/*
	 * The AuthString will be splitted into fields and stored.
	 */
	void set (const std::string &AuthString);

	/*
	 * Returns the field[0]. Per Definition is filed[0] the
	 * AuthenticationType.
	 * \param AuthString the authentication string (unix:*:*::debug)
	 */
	const std::string& getType (void);

	/*
	 * Returns the value of field with the number nr.
	 */
	const std::string& getField(int nr);

	/*
	 * Returns the number of stored fields.
	 */
	int getNrOfFields (void);

	/*
	 * Modify (append) the specified field with the value.
	 * \param nr Fieldnumber
	 * \param v value
	 * \throw bounding error of type char
	 */
	void modifyField (unsigned int nr, const char *v);

	/*
	 * Modify the current object with the source.
	 * Walk through the lists and append each 
	 * destination field, with the source field.
	 * \param start start with element nr start
	 * \param source of modify operation
	 */
	void modify (unsigned int start, Authentication &source);

	/*
	 * Append the next field with the value.
	 * \param v value
	 */
	void appendField (const char *v);

	/*
	 * print the actual setting to std:ostream
	 */
	void printParameters (std::ostream *pOut) const;

	/*
	 * Type equal and ignore a : on the last position
	 * \return 0 ... equal 
	 */
	int typeEqual (const std::string &v);

  private:
	std::vector<std::string> fields;
	std::string defAuth;
};

/**
 * \class AccessEntry
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class AccessEntry {
      public:
	std::string hostname;
	struct sockaddr_storage addr;
	unsigned short prefixlen;

	enum {
		af_read = 0x1,
		af_post = 0x2,
		af_debug = 0x4,
		af_authentication = 0x8,
	};

	unsigned int access_flags;

	NewsgroupFilter list;	//! groups visible in active db
	NewsgroupFilter read;	//! groups that clients may read
	NewsgroupFilter postTo;	//! groups that clients may post to
	Authentication authentication;
	std::string PAMServicename;  //! PAM Service name for this Client

	 AccessEntry() {
		init();
	} void init() {
		addr.ss_family = AF_UNSPEC;
		prefixlen = 0;
		access_flags = 0x0;
		PAMServicename = PAM_DEFAULT_SERVICENAME;
	}

	void clear() {
		init();
	}
	void modifyAccessFlags (const std::string &flags);
	friend std::ostream & operator<<(std::ostream & os,
					 const AccessEntry & ae);

	/*
	 * print the actual setting to std:ostream
	 * \author Herbert Straub
	 * \date 2003
	 */
	void printParameters (std::ostream *pOut);

	/*
	 * Print the value of access_flags (translated from 
	 * Hex Values to String) to std::ostream.
	 * \author Herbert Straub
	 * \date 2003
	 */
	void printAccessFlags (std::ostream *pOut) const;
};

/**
 * \class AccessList
 * \author Thomas Gschwind
 *
 * \bug Documentation is missing.
 */
class AccessList {
      private:
	std::vector < AccessEntry > vector;

      public:
	AccessList() {
	}
	AccessEntry *client(const char *name, const struct sockaddr *addr,
			    socklen_t addrlen);
	void init() {
		vector.clear();
	}
	void read(Lexer & lex);
	void readClient(Lexer & lex, const char *address);

	/*
	 * print the actual setting to std:ostream
	 * \author Herbert Straub
	 * \date 2003
	 */
	void printParameters (std::ostream *pOut);
};


// Inline methods
inline const std::string& Authentication::getType (void)
{
	if (fields.size() == 0) {
		return defAuth;
	} else {
		return fields[0];
	}
}

inline const std::string& Authentication::getField(int nr) 
{
	return fields[nr];
}
inline int Authentication::getNrOfFields (void)
{
	return fields.size();
}
inline void AccessEntry::printAccessFlags (std::ostream *pOut) const
{
	if (access_flags == 0x0) {
		*pOut << " none";
	}
	if (access_flags & af_read) {
		*pOut << " read";
	}
	if (access_flags & af_post) {
		*pOut << " post";
	}
	if (access_flags & af_debug) {
		*pOut << " debug";
	}
}
inline void Authentication::modifyField (unsigned int nr, const char *v)
{
	if (nr >= fields.size())
		throw ("bounding error");

	fields[nr] += v;
}

inline void Authentication::appendField (const char *v)
{
	fields.push_back (v);
}
#endif
