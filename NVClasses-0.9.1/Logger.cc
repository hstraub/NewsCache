#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#ifndef WITH_SYSLOG
#include<unistd.h>
#include<time.h>
#include<sys/time.h>
#endif

#include"Logger.h"

using namespace std;

// private:
int Logger::bufresize(int sz)
{
	int nsz;

	/* Round up to the next 4k boundary */
	nsz = (sz + 0x1000) & ~0xfff;
	if (nsz != bufsz) {
		/* Add Space */
		if (buf)
			buf = (char *) realloc(buf, nsz);
		else
			buf = (char *) malloc(nsz);
		if (!buf) {
			bufsz = 0;
			return 0;
		}
		bufsz = nsz;
	}
	return bufsz;
}

void Logger::append(const char *s)
{
	int slen = strlen(s);
	if (bufresize(buflen + slen + 1) == 0)
		exit(26);
	strcat(buf, s);
	buflen += slen;
}

void Logger::print()
{
	char *nl, *p;

	while ((nl = strchr(buf, '\n')) != NULL) {
		// Print first line
		*nl = '\0';
#ifdef WITH_SYSLOG
		syslog(_priority, "%s", buf);
#else
		time_t ts;
		struct tm *t;
		char tbuf[32];
		time(&ts);
		t = localtime(&ts);
		sprintf(tbuf, "%d %04d/%02d/%02d %02d:%02d:%02d ",
			_priority, 1900 + t->tm_year, t->tm_mon,
			t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		log << tbuf << buf << endl;
#endif

		// Move remaining string to the beginning
		nl++;
		p = buf;
		while ((*p++ = *nl++) != '\0');
	}
	buflen = strlen(buf);
}

// public:
#ifdef WITH_SYSLOG
Logger::Logger()
{
	buf = NULL;
	bufsz = 0;
	if (bufresize(1) == 0)
		exit(26);
	buf[0] = '\0';
	buflen = 0;
	_priority = Debug;
}

void Logger::open(char *ident, int option, int facility)
{
	openlog(ident, option, facility);
}

Logger::~Logger()
{
	close();
}

void Logger::close()
{
	closelog();
}

#else
Logger::Logger(char *fn, int option)
{
	buf = NULL;
	bufsz = 0;
	if (bufresize(1) == 0)
		exit(26);
	buf[0] = '\0';
	buflen = 0;
	_priority = Debug;

	open(fn);
}

Logger::~Logger()
{
	close();
}

void Logger::open(char *fn)
{
	if (fn) {
		log.open(fn, ios::app);
		log.setf(ios::unitbuf);
	}
}

void Logger::close()
{
	log.close();
}
#endif

Logger & Logger::priority(int p)
{
	_priority = p;
	if (buflen && buf[buflen - 1] != '\n')
		write("\n");
	return *this;
}

Logger & Logger::write(const char *s)
{
	append(s);
	print();
	return *this;
}

// friends:
Logger & operator<<(Logger & l, const char *s)
{
	return l.write(s);
}

Logger & operator<<(Logger & l, const string & s)
{
	string s2(s);
	return l.write(s2.c_str());
}

Logger & operator<<(Logger & l, char ch)
{
	return l.write(ch);
}

Logger & operator<<(Logger & l, unsigned int i)
{
	char s[256];
	sprintf(s, "%u", i);
	return l.write(s);
}

Logger & operator<<(Logger & l, int i)
{
	char s[256];
	sprintf(s, "%d", i);
	return l.write(s);
}
