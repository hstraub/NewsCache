#if !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__))
#include <crypt.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <string>

char getsaltchar()
{
	int r = (int) (64.0 * rand() / (RAND_MAX + 1.0));
	if (r < 26)
		return 'a' + r;
	r -= 26;
	if (r < 26)
		return 'A' + r;
	r -= 26;
	if (r < 10)
		return '0' + r;
	if (r < 1)
		return '.';
	else
		return '/';
}

int main(int argc, char *argv[])
{
	char salt[2];
	int i;

	if (argc != 2 || string(argv[1]) == "-?") {
		cerr << "Usage: " << argv[0] << " passwd" << endl;
		exit(1);
	}

	srand(time(NULL));
	salt[0] = getsaltchar();
	salt[1] = getsaltchar();

	cout << "crypt: " << crypt(argv[1], salt) << endl;
}

/*
 * Local Variables:
 * mode: c++
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
