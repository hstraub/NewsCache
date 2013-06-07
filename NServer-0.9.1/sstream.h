#ifndef __sstream_h__
#define __sstream_h__

/*
 * Copyright (C) 2002 Herbert Straub
 * Change the original hierachy [(C) Thomas Gschwind] 
 * from fstream -> sstream to iosockinet -> sstream
 * This is for g++ >= 3.0 necessary.
 */


#include <socket++/sockinet.h>

/**
 * \class sstream
 * \author Herbert Straub
 * \date 2002
 * Change the original hierachy [(C) Thomas Gschwind] 
 * from fstream -> sstream to iosockinet -> sstream
 * This is for g++ >= 3.0 necessary.
 *
 * \bug Documentation is missing.
 */
class sstream:public iosockinet {
	char _name[256];
	char _service[256];
	int connected;
      public:
	 sstream():iosockinet(sockbuf::sock_stream) {
		_name[0] = '\0';
		_service[0] = '\0';
		connected = 0;
		setstate(std::ios::badbit);
		rdbuf()->setname ("upstream server socket");
	} sstream(char *name,
		  char *service):iosockinet(sockbuf::sock_stream) {
		rdbuf()->setname ("upstream server socket");
		connectTo(name, service);
	}
	sstream(char *name, char *service,
		char *from):iosockinet(sockbuf::sock_stream) {
		rdbuf()->setname ("upstream server socket");
		connectTo(name, service, from);
	}
	~sstream() {
		disconnect();
	}

	const char *hostname() {
		return _name;
	}
	const char *servicename() {
		return _service;
	}
	int is_connected();
	void connectTo(char *name, char *service);
	void connectTo(char *name, char *service, char *from);
	void disconnect();
};

#endif
