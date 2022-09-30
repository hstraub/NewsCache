#ifndef MD5_H
#define MD5_H

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __alpha
	typedef unsigned int uint32;
#else
	typedef unsigned long uint32;
#endif

	struct MD5Context {
		uint32 buf[4];
		uint32 bits[2];
		unsigned char in[64];
	};

	void MD5Init(struct MD5Context *context);
	void MD5Update(struct MD5Context *context,
		       unsigned char const *buf, unsigned len);
	void MD5Final(unsigned char digest[16],
		      struct MD5Context *context);
	void MD5Transform(uint32 buf[4], uint32 const in[16]);

	typedef struct MD5Context MD5_CTX;
#ifdef __cplusplus
}
#endif
#endif				/* !MD5_H */

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
