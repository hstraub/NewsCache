#ifndef __OverviewFmt_h__
#define __OverviewFmt_h__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "config.h"
#include "NSError.h"
#include "Debug.h"
#include "Article.h"
#include "readline.h"

/**
 * CONF_OverviewFmt
 * This defines the format of your overview-database. The overview
 * database stores the most important data for each article
 * (subject, date, lines, ...). Set this to the format of the
 * news server being most heavily used. You may find out this
 * format using the following commands
 * telnet news.server nntp
 * mode reader
 * list overview.fmt
 * quit
 * The optional full keyword must be written in small letters!!!
 */
#define CONF_OverviewFmt {"Subject:","From:","Date:",\
       "Message-ID:","References:","Bytes:","Lines:",\
       "Xref:full",NULL}

/**
 * \class OverviewFmt
 * \author Thomas Gschwind
 */
class OverviewFmt {
      private:
	struct overdesc {
		char name[256];	// Name of field includeing :
		int len;	// strlen(name)
		int full;	// 1 full header, 0 exclude name of field
	};
	struct transtab {
		int trans, full;
	};
	/* xoout ... Output format of overview-database
	 * xohdrs ... Headers of overview-database --- same as xoout, except
	 *            the full suffix
	 * trans[i] ... Position of i-th overview record field (in my format) 
	 *              in incoming records
	 * transflg ... Indicates, whether the header has to be prepended
	 *              or stripped.
	 * dotrans ... Indicates, whether incoming database records have
	 *             to be translated
	 */
	overdesc _over[128];
	transtab _trans[128];
	int _osz;

      public:
	int dotrans;
	 OverviewFmt() {
		const char *tmp[] = CONF_OverviewFmt, *q;
		char *p;
		for (_osz = 0; tmp[_osz]; _osz++) {
			p = _over[_osz].name;
			q = tmp[_osz];
			while ((*p++ = *q++) != ':');
			*p = '\0';
			_over[_osz].len = p - _over[_osz].name;
			_over[_osz].full = (*q == 'f') ? 1 : 0;
		} dotrans = 0;
	}

	/**
	 * Return a field of the overview database.
	 * \param over An overview record.
	 * \param fld The name of the field to be extracted.
	 * \param full
	 * 	\li -1 => do not strip/add fieldname
	 * 	\li 0 => strip fieldname
	 * 	\li 1 => add fieldname
	 */
	std::string getfield(const char *over, const char *fld, int full = -1) {
		std::string field;
		int i;

		for (i = 0; i < _osz; i++) {
			if (strcasecmp(fld, _over[i].name) == 0) {
				int j = i + 1;
				const char *p = over, *q;

				while (*p && j) {
					if (*p++ == '\t')
						j--;
				}
				q = p;
				while (*q && *q != '\t')
					q++;
				if (full == -1 || full == _over[i].full) {
					// No fieldname needs to be added/stripped
					field.assign(p, q - p);
					return field;
				}
				if (full < _over[i].full) {
					// Strip fieldname
					while (*p && *p != ':')
						p++;
					while (*p && isspace(*p))
						p++;
					field.assign(p, q - p);
					return field;
				}
				// Add fieldname
				field = _over[i].name;
				field += ' ';
				field.append(p, q - p);
				return field;
			}
		}
		throw NoSuchFieldError(fld, ERROR_LOCATION);
	}

	/**
	 * Read overview format and set up conversion table.
	 * \param is Input stream where the overview database is read from.
	 */
	void readxoin(std::istream & is);

	/**
	 * Convert from server overview format to local overview format
	 * if necessary.
	 * \param recin The overview record that has to be converted.
	 * \param recout The converted overview record.
	 */
	void convert(const std::string & recin, std::string & recout) const {
		int tabs[256];
		int i, j, k, rinsz;
		if (!dotrans) {
			recout = recin;
			return;
		}

		i = 0;
		rinsz = 0;
		j = recin.length();
		do {
			if (recin[i] == '\t')
				tabs[rinsz++] = i;
			i++;
		} while (i < j);
		tabs[rinsz] = i;

		recout = recin.substr(0, tabs[0]);
		for (i = 0; i < _osz; i++) {
			j = _trans[i].trans;
			if (j >= rinsz) {
				// FIXME! recin has fewer values than it should have
				// FIXME! Is this an error?
				// FIXME! Assuming that the missing ones are empty
				recout += '\t';
			} else if (j >= 0) {
				if (_trans[i].full == _over[i].full) {
					// Leave field as is
					recout +=
					    recin.substr(tabs[j],
							 tabs[j + 1] -
							 tabs[j]);
				} else if (_trans[i].full > _over[i].full) {
					// Strip fieldname from field
					k = tabs[j];
					while (recin[k] != ':'
					       && k < tabs[j + 1])
						k++;
					if (recin[k] != ':') {
						// Cannot find fieldname -- leave it as it is
						recout +=
						    recin.substr(tabs[j],
								 tabs[j +
								      1] -
								 tabs[j]);
					} else {
						while (isspace(recin[k])
						       && k < tabs[j + 1])
							k++;
						recout += '\t';
						recout +=
						    recin.substr(k,
								 tabs[j +
								      1] -
								 k);
					}
				} else {
					// Add fieldname to field
					recout += '\t';
					recout += _over[i].name;
					recout += ' ';
					recout +=
					    recin.substr(tabs[j] + 1,
							 tabs[j + 1] -
							 tabs[j] - 1);
				}
			} else {
				recout += '\t';
			}
		}
	}

	void convert(const Article & article, std::string & recout) const {
		char buf[256];
		std::string fld;
		int i;

		 sprintf(buf, "%d", article.getnbr());
		 recout = buf;
		for (i = 0; i < _osz; i++) {
			recout += '\t';
			try {
				recout.append(article.
					      getfield(_over[i].name,
						       _over[i].full));
			}
			catch(NoSuchFieldError & nf) {
				// Check, whether field==bytes:
				if (strcasecmp(_over[i].name, "bytes:") ==
				    0) {
					sprintf(buf, "%d",
						article.length());
					recout.append(buf);
				}
				// Field not found => empty
			}
		}
	}

	friend std::ostream & operator <<(std::ostream & os,
					  OverviewFmt & o) {
		int i;
		for (i = 0; i < o._osz; i++) {
			os << o._over[i].name;
			if (o._over[i].full)
				os << "full\r\n";
			else
				os << "\r\n";
		}
		return os;
	}

};

#endif
