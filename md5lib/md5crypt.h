#ifndef MD5_CRYPT_H
#define MD5_CRYPT_H

#include <string.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif
	extern char *md5crypt(const char *password);
	extern int b64_ntop(u_char const *src, size_t srclength,
			    char *target, size_t targsize);
#ifdef __cplusplus
}
#endif
#endif				/*MD5_CRYPT_H */

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
