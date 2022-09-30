#ifndef __sstream_h__
#define __sstream_h__

#include <signal.h>
#include <sys/socket.h>

#include <iostream>

class sockstream
  : public std::iostream
{
  class sockstreambuf
    : public std::streambuf
  {
    static sig_atomic_t *_alarm_indicator;

    int _fd;
    char _gbuf[4096];
    char _pbuf[4096];

   public:
    static inline void alarm_indicator(sig_atomic_t &var)
    {
      _alarm_indicator = &var;
    }

    sockstreambuf()
      : _fd(-1)
    {
      setg(_gbuf, _gbuf + sizeof(_gbuf), _gbuf + sizeof(_gbuf));
      setp(_pbuf, _pbuf + sizeof(_gbuf));
    }

    ~sockstreambuf()
    { disconnect(); }

    inline bool connected() const
    { return _fd >= 0; }

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

  inline sockstream()
    : std::iostream(&_buf)
  {
    setstate(std::ios::badbit);
  }

  inline sockstream(const char *name, const char *service,
                    const struct sockaddr *from = NULL, socklen_t fromlen = 0)
    : std::iostream(&_buf)
  {
    _buf.connect(name, service, from, fromlen);
  }

  inline ~sockstream()
  { }

  inline bool connected() const
  { return _buf.connected(); }

  inline void connect(const char *name, const char *service,
		      const struct sockaddr *from = NULL,
		      socklen_t fromlen = 0, unsigned int timeout = 0)
  {
    _buf.connect(name, service, from, fromlen, timeout);
    clear(_buf.connected() ? std::ios::goodbit : std::ios::badbit);
  }

  inline void connect(const char *name, const char *service,
		      unsigned int timeout = 0)
  {
    _buf.connect(name, service, NULL, 0, timeout);
    clear(_buf.connected() ? std::ios::goodbit : std::ios::badbit);
  }

  inline void attach(int fd)
  {
    _buf.attach(fd);
    clear();
  }

  inline void disconnect()
  {
    _buf.disconnect();
    clear(std::ios::badbit);
  }

  inline void setkeepalive(bool fl = true)
  { _buf.setkeepalive(fl); }

  inline void setnodelay(bool fl = true)
  { _buf.setnodelay(fl); }
};

#endif
