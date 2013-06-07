#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "Debug.h"
#include "sstream.h"

using namespace std;

void sstream::connectTo(char *name, char *service)
{
	connectTo(name, service, "");
}

void sstream::connectTo(char *name, char *service, char *from)
{
	char buf[1024];

	if (is_connected())
		disconnect();
	strcpy(_name, name);
	strcpy(_service, service);

	try {
		if ((strcmp(from, "") != 0)) {
			VERB(slog.
			     p(Logger::
			       Debug) <<
			     "sstream::connectTo using interface " << from
			     << "\n");
			sockinetaddr bindAddr(from);
			(*((iosockinet *) this))->bind(bindAddr);
		}
	}
	catch(sockerr e) {
		sprintf(buf, "can't bind to %s\n", from);
		slog.
		    p(Logger::
		      Error) << "sstream::connectTo: can't bind to " <<
		    from << " sockerr code: " << e.serrno()
		    << " sockerr operation: " << e.operation()
		    << " sockerrtext: " << e.errstr() << "\n";
		setstate(ios::badbit);
		return;
	}

	try {
		VERB(slog.
		     p(Logger::
		       Debug) << "sstream::connectTo connecting to " <<
		     name << " service " << service << "\n");
		sockinetaddr serverAddr(name, service);
		(*this)->connect(serverAddr);
	}
	catch(sockerr e) {
		sprintf(buf, "cannot connect to %s (%s)\n", name, service);
		slog.
		    p(Logger::
		      Error) << "sstream::connectTo can't connect to " <<
		    name << " " << service << " sockerr code: " << e.
		    serrno() << " sockerr operation: " << e.operation()
		    << " sockerrtext: " << e.errstr() << "\n";
		setstate(ios::badbit);
		return;
	}

	try {
		(*this)->recvtimeout(-1);
		//(*this)->sendtimeout (-1);
		(*this)->keepalive(1);
	}
	catch(sockerr e) {
		slog.
		    p(Logger::
		      Error) <<
		    "sstream::connectTo: setting keepalive error " <<
		    " sockerr code: " << e.
		    serrno() << " sockerr operation: " << e.operation()
		    << " sockerrtext: " << e.errstr() << "\n";
		// FIXME: is this a fatal error?
	}

	connected = 1;
	clear();
}

void sstream::disconnect()
{
	sockbuf *pS;

	VERB(slog.p(Logger::Debug) << "sstream::disconnect()\n");
	try {
		pS = rdbuf();
		pS->shutdown(sockbuf::shut_readwrite);
	} catch (sockerr e) {
		VERB(slog.p(Logger::Debug) << "sstream::disconnect() sockerror"
			<< e.serrno() << " sockerr operation: "
			<< e.operation() << " sockerrtext: " << e.errstr()
			<< "\n");
	}
	_name[0] = '\0';
	_service[0] = '\0';
	connected = 0;
	setstate(ios::badbit);
}

int sstream::is_connected()
{
	return connected;
}
