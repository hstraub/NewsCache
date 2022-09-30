#include "OverviewFmt.h"

using namespace std;

void OverviewFmt::readxoin(istream & is)
{
	string field;
	int fn = 0, i;

	for (i = 0; i < _osz; i++) {
		_trans[i].trans = -1;
	}

	for (;;) {
		nlreadline(is, field, 0);
		if (field == "." || is.eof())
			break;
		for (i = 0; i < _osz; i++) {
			if (field.length() >= (unsigned int) _over[i].len
			    && strncasecmp(field.data(), _over[i].name,
					   _over[i].len) == 0) {
				if (field.length() ==
				    (unsigned int) _over[i].len + 4
				    && strncasecmp("full",
						   field.data() +
						   _over[i].len, 4) == 0) {
					_trans[i].full = 1;
				} else {
					_trans[i].full = 0;
				}
				_trans[i].trans = fn;
			}
		}
		fn++;
	}
	dotrans = 0;
	if (_osz != fn) {
		dotrans = 1;
	} else {
		for (i = 0; i < _osz; i++) {
			if (_trans[i].trans != i
			    || _trans[i].full != _over[i].full) {
				dotrans = 1;
				break;
			}
		}
	}
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
