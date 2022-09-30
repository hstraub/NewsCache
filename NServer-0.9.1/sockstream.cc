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


namespace {
#if defined(HAVE_GNUTLS)
struct Gnutls_Initer
{
  Gnutls_Initer()
  {
    gnutls_global_init();
  }

  ~Gnutls_Initer()
  {
    gnutls_global_deinit();
  }
} gnutls_initer;
#endif
}


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


#if defined(HAVE_GNUTLS)
sockstream::sslstreambuf::sslstreambuf(sockstreambuf &buf)
  : _buf(buf), _connected(false)
{
  setg(_buf._gbuf, _buf._gbuf + sizeof(_buf._gbuf),
       _buf._gbuf + sizeof(_buf._gbuf));
  setp(_buf._pbuf, _buf._pbuf + sizeof(_buf._pbuf));

  gnutls_init(&_session, GNUTLS_CLIENT);

  if (gnutls_anon_allocate_client_credentials(&_anon_cred) < 0)
  {
    gnutls_deinit(_session);
  }

  if (gnutls_certificate_allocate_credentials(&_cert_cred) < 0)
  {
    gnutls_anon_free_client_credentials(_anon_cred);
    gnutls_deinit(_session);
  }

  // set up SSL/TLS parameters
  static const int cipher_priority[] = {
    GNUTLS_CIPHER_AES_128_CBC,
    GNUTLS_CIPHER_AES_256_CBC,
    GNUTLS_CIPHER_ARCFOUR_128,
    GNUTLS_CIPHER_3DES_CBC,
    0 
  };
  gnutls_cipher_set_priority(_session, cipher_priority);

  static const int compression_priority[] = {
    GNUTLS_COMP_ZLIB,
    GNUTLS_COMP_NULL,
    0
  };
  gnutls_compression_set_priority(_session, compression_priority);

  static const int kx_priority[] = {
    GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS,
    GNUTLS_KX_DHE_RSA, GNUTLS_KX_SRP,
    GNUTLS_KX_ANON_DH, GNUTLS_KX_RSA_EXPORT,
    0
  };
  gnutls_kx_set_priority(_session, kx_priority);

  static const int protocol_priority[] = {
    GNUTLS_TLS1, GNUTLS_SSL3, 0
  };
  gnutls_protocol_set_priority(_session, protocol_priority);


  static const int mac_priority[] = {
    GNUTLS_MAC_MD5, GNUTLS_MAC_SHA, 0
  };
  gnutls_mac_set_priority(_session, mac_priority);

  static const int cert_type_priority[] = {
    GNUTLS_CRT_X509,
    GNUTLS_CRT_OPENPGP, 0
  };
  gnutls_certificate_type_set_priority(_session, cert_type_priority);

  gnutls_credentials_set(_session, GNUTLS_CRD_ANON, _anon_cred);
  gnutls_credentials_set(_session, GNUTLS_CRD_CERTIFICATE, _cert_cred);
}

sockstream::sslstreambuf::~sslstreambuf()
{
  disconnect();

  gnutls_deinit(_session);

  gnutls_anon_free_client_credentials(_anon_cred);
  gnutls_certificate_free_credentials(_cert_cred);
}


void sockstream::sslstreambuf::connect()
{
  gnutls_transport_set_ptr(_session,
			   reinterpret_cast<gnutls_transport_ptr>(_buf.fd()));

  int rc_connect = GNUTLS_E_AGAIN;
  while (rc_connect == GNUTLS_E_AGAIN)
  {
    rc_connect = gnutls_handshake(_session);
  }

  _connected = !rc_connect;
}

void sockstream::sslstreambuf::disconnect()
{
  int rc_shutdown = GNUTLS_E_AGAIN;
  while (rc_shutdown == GNUTLS_E_AGAIN)
  {
    rc_shutdown = gnutls_bye(_session, GNUTLS_SHUT_RDWR);
  }
}


int sockstream::sslstreambuf::overflow(int c)
{
  if (!_connected || !_buf.connected())
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

int sockstream::sslstreambuf::sync()
{
  size_t off = 0;

  while ((pbase() + off) != pptr())
  {
    const int rc_write =
      gnutls_write(_session, pbase() + off, pptr() - pbase() - off);
    if (rc_write < 0)
    {
      return -1;
    }

    off += rc_write;
  }

  setp(_buf._pbuf, _buf._pbuf + sizeof(_buf._pbuf));

  return 0;
}

int sockstream::sslstreambuf::underflow()
{
  if (gptr() < egptr())
  {
    return *gptr();
  }

  const int rc_read = gnutls_read(_session, _buf._gbuf, sizeof(_buf._gbuf));
  if (rc_read > 0)
  {
    setg(_buf._gbuf, _buf._gbuf, _buf._gbuf + rc_read);
    return (unsigned char) *gptr();
  }
  else
  {
    return traits_type::eof();
  }
}
#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
