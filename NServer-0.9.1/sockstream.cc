#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "config.h"
#include "Debug.h"
#include "sockstream.h"


sig_atomic_t *sockstream::sockstreambuf::_alarm_indicator = NULL;


void sockstream::sockstreambuf::connect(const char *name, const char *service,
					const struct sockaddr *from,
					socklen_t fromlen,
					unsigned int timeout)
{
  disconnect();

  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_ADDRCONFIG | ((service[0] == '#') ? AI_NUMERICSERV : 0);
  hints.ai_family = (from != NULL) ? from->sa_family : AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo *res;
  if (getaddrinfo(name, (service[0] == '#') ? service + 1 : service,
		  &hints, &res))
  {
    slog.p(Logger::Error) << "Can't resolve " << name << ' ' << service
			  << "\n";
    return;
  }

  struct addrinfo *addr = res;
  while ((_fd < 0) && (addr != NULL))
  {
    _fd = ::socket(addr->ai_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (_fd >= 0)
    {
      if (from != NULL)
      {
	if (::bind(_fd, from, fromlen) < 0)
	{
	  slog.p(Logger::Error) << "Unable to bind to outgoing address"
				<< "\n";

	  while (::close(_fd) && (errno == EINTR))
	  { }
	  _fd = -1;
	  freeaddrinfo(res);

	  return;
	}
      }

      if (timeout && _alarm_indicator)
      {
	*_alarm_indicator = 0;
	alarm(timeout);
      }

      int rc_connect;
      while (((rc_connect = ::connect(_fd, addr->ai_addr,
				      addr->ai_addrlen)) < 0) &&
	     (errno == EINTR) &&
	     (!timeout || !_alarm_indicator || !*_alarm_indicator))
      { }

      if (timeout && _alarm_indicator)
      {
	alarm(0);
      }

      if (rc_connect)
      {
	if (timeout && _alarm_indicator && *_alarm_indicator)
	{
	  *_alarm_indicator = 0;
	  slog.p(Logger::Notice) << "Connection attempt to " << name
				 << " timed out\n";
	}
	else
	{
	  slog.p(Logger::Notice) << "Connection attempt to " << name
				 << " failed: " << strerror(errno) << "\n";
	}
      
	while (::close(_fd) && (errno == EINTR))
	{ }
	_fd = -1;
      }
    }

    addr = addr->ai_next;
  }

  if (_fd < 0)
  {
    slog.p(Logger::Warning) << "Unable to connect to " << name << ' '
			    << service << "\n";
  }

  freeaddrinfo(res);
}

void sockstream::sockstreambuf::attach(int fd)
{
  disconnect();
  _fd = fd;
}

void sockstream::sockstreambuf::disconnect()
{
  if (connected())
  {
    sync();
    setg(_gbuf, _gbuf + sizeof(_gbuf), _gbuf + sizeof(_gbuf));
    setp(_pbuf, _pbuf + sizeof(_gbuf));

    while (::close(_fd) && (errno == EINTR))
    { }

    _fd = -1;
  }
}

void sockstream::sockstreambuf::setkeepalive(bool fl)
{
  const int value = fl;

  if (setsockopt(_fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) < 0)
  {
    slog.p(Logger::Error) << "setsockopt failed: "
			  << strerror(errno) << "\n";
  }
}

void sockstream::sockstreambuf::setnodelay(bool fl)
{
  const int value = fl;
  if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) < 0)
  {
    slog.p(Logger::Error) << "setsockopt failed: "
			  << strerror(errno) << "\n";
  }
}


int sockstream::sockstreambuf::overflow(int c)
{
  if (_fd < 0)
  {
    return traits_type::eof();
  }

  if (c == traits_type::eof())
  {
    return sync() ? traits_type::eof() : 0;
  }

  if (pptr() == epptr())
  {
    if (sync())
    {
      return traits_type::eof();
    }
  }

  *pptr() = char(c);
  pbump(1);

  return c;
}

int sockstream::sockstreambuf::sync()
{
  size_t off = 0;

  while ((pbase() + off) != pptr())
  {
    int rc_send;
    do
    {
      rc_send = ::send(_fd, pbase() + off, pptr() - pbase() - off, 0);
    } while ((rc_send < 0) && (errno == EINTR) &&
	     (!_alarm_indicator || !*_alarm_indicator));

    if (rc_send < 0)
    {
      return -1;
    }

    off += rc_send;
  }

  setp(_pbuf, _pbuf + sizeof(_pbuf));

  return 0;
}

int sockstream::sockstreambuf::underflow()
{
  if (gptr() < egptr())
  {
    return *gptr();
  }

  int rc_recv;
  do
  {
    rc_recv = ::recv(_fd, _gbuf, sizeof(_gbuf), 0);
  } while ((rc_recv < 0) && (errno == EINTR) &&
	   (!_alarm_indicator || !*_alarm_indicator));

  if (rc_recv > 0)
  {
    setg(_gbuf, _gbuf, _gbuf + rc_recv);
    return (unsigned char) *gptr();
  }
  else
  {
    return traits_type::eof();
  }
}
