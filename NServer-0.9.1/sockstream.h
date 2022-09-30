#ifndef __sstream_h__
#define __sstream_h__

#include <signal.h>
#include <sys/socket.h>

#if defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#endif

#include <iostream>


class sockstream
  : public std::iostream
{
  class sockstreambuf;

  class sslstreambuf
    : public std::streambuf
  {
#if defined(HAVE_GNUTLS)
    sockstreambuf &_buf;
    bool _connected;

    gnutls_session _session;
    gnutls_anon_client_credentials _anon_cred;
    gnutls_certificate_credentials _cert_cred;

   public:
    explicit sslstreambuf(sockstreambuf &buf);

    ~sslstreambuf();

    virtual int overflow(int c = traits_type::eof());
    virtual int sync();

    virtual int underflow();

    inline bool connected() const
    { return _connected; }

    void connect();
    void disconnect();
#else
   public:
    inline bool connected() const
    { return false; }

    inline void connect()
    { }
    inline void disconnect()
    { }
#endif
  };

  sslstreambuf * const _ssl;

  class sockstreambuf
    : public std::streambuf
  {
    friend class sslstreambuf;

    static sig_atomic_t *_alarm_indicator;

    int _fd;
    char _gbuf[4096];
    char _pbuf[4096];

   public:
    static inline void alarm_indicator(sig_atomic_t &var)
    {
      _alarm_indicator = &var;
    }

    inline sockstreambuf()
      : _fd(-1)
    {
      setg(_gbuf, _gbuf + sizeof(_gbuf), _gbuf + sizeof(_gbuf));
      setp(_pbuf, _pbuf + sizeof(_pbuf));
    }

    inline ~sockstreambuf()
    { disconnect(); }

    inline bool connected() const
    { return _fd >= 0; }

    inline int fd() const
    { return _fd; }

    void connect(const char *name, const char *service,
		 const struct sockaddr *from, socklen_t fromlen,
		 unsigned int timeout = 0);

    void attach(int fd);

    void disconnect();

    void setkeepalive(bool fl = true);
    void setnodelay(bool fl = true);

    virtual int overflow(int c = traits_type::eof());
    virtual int sync();

    virtual int underflow();
  } _buf;

 public:
  static inline void alarm_indicator(sig_atomic_t &var)
  {
    sockstreambuf::alarm_indicator(var);
  }

  inline explicit sockstream(const bool ssl = false)
#if defined(HAVE_GNUTLS)
    : std::iostream(ssl ?
		    static_cast<std::streambuf *>(new sslstreambuf(_buf)) :
		    static_cast<std::streambuf *>(&_buf)),
      _ssl(ssl ? static_cast<sslstreambuf *>(rdbuf()) : NULL)
#else
    : std::iostream(static_cast<std::streambuf *>(&_buf)),
      _ssl(NULL)
#endif
  {
    setstate(std::ios::badbit);
  }

  inline sockstream(const char *name, const char *service,
                    const struct sockaddr *from = NULL, socklen_t fromlen = 0)
    : std::iostream(&_buf), _ssl(NULL)
  {
    _buf.connect(name, service, from, fromlen);
  }

  inline ~sockstream()
  {
    if (_ssl)
    {
      delete _ssl;
    }
  }

  inline bool connected() const
  {
    return _ssl ? _ssl->connected() : _buf.connected();
  }

  inline void connect(const char *name, const char *service,
		      const struct sockaddr *from = NULL,
		      socklen_t fromlen = 0, unsigned int timeout = 0)
  {
    _buf.connect(name, service, from, fromlen, timeout);
    if (_ssl && _buf.connected()) _ssl->connect();
    clear((_ssl ? _ssl->connected() : _buf.connected()) ?
	  std::ios::goodbit : std::ios::badbit);
  }

  inline void connect(const char *name, const char *service,
		      unsigned int timeout = 0)
  {
    _buf.connect(name, service, NULL, 0, timeout);
    if (_ssl && _buf.connected()) _ssl->connect();
    clear((_ssl ? _ssl->connected() : _buf.connected()) ?
	  std::ios::goodbit : std::ios::badbit);
  }

  inline void attach(int fd)
  {
    _buf.attach(fd);
    clear();
  }

  inline void disconnect()
  {
    if (_ssl)
    {
      _ssl->disconnect();
    }

    _buf.disconnect();
    clear(std::ios::badbit);
  }

  inline void setkeepalive(bool fl = true)
  { _buf.setkeepalive(fl); }

  inline void setnodelay(bool fl = true)
  { _buf.setnodelay(fl); }
};

#endif

/*
 * Local Variables:
 * mode: c++
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
