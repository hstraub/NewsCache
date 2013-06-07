#include <iostream>
#include <fstream>
#include <string>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "Logger.h"
#include "Article.h"

using namespace std;

char *articleList[] = { "article5283", "article9621", "article9622" };

Logger slog;
Article art1;

int checkArticle(const char *artfn)
{
	char buf[256];
	int la1;
	ifstream is;
	ofstream os;
	Article *art2;

	sprintf(buf, "%s.in", artfn);
	is.open(buf);
	art1.read(is);
	is.close();
	la1 = art1.length();

	art2 = new Article(art1);
	if (strcmp(art1.c_str(), art2->c_str()))
		return -11;
	if (la1 != art1.length() || la1 != art2->length())
		return -12;

	art1.settext(art2->gettext());
	if (strcmp(art1.c_str(), art2->c_str()))
		return -21;
	if (la1 != art1.length() || la1 != art2->length())
		return -22;

	strcpy(buf, artfn);
	strcat(buf, ".out");
	os.open(buf);
	art2->write(os);
	os.close();

	sprintf(buf, "%s.in", artfn);
	is.open(buf);
	art1.read(is);
	is.close();
	la1 = art1.length();

	delete art2;
	art2 = new Article(0, art1.c_str(), art1.length());
	if (strcmp(art1.c_str(), art2->c_str()))
		return -31;
	if (la1 != art1.length() || la1 != art2->length())
		return -32;

	if (!art1.has_field("from:"))
		return -41;
	if (!art2->has_field("FROM:"))
		return -42;

	if (art1.has_field("foobar:"))
		return -51;

	//Xref: news.tuwien.ac.at at.tuwien.admins:892 at.tuwien.tunet:1520 at.tuwien.general:5283 at.tuwien.edvz.neuigkeiten:531

	if (art1.getfield("dummy:") != "dummy")
		return -61;
	if (art2->getfield("DuMmY:", 1) != "dummy: dummy")
		return -71;

	art1.setfield("dummy:", "dummy: dummy2\r\n");
	art1.setfield("dummy2:", "dummy2: dummy2\r\n");
	if (!strcmp(art1.c_str(), art2->c_str()))
		return -81;
	if (la1 == art1.length())
		return -82;
	if (art1.getfield("dummy:") != "dummy2")
		return -83;
	if (art1.getfield("dummy2:") != "dummy2")
		return -84;

	return 0;
}

int main(int argc, char *argv[])
{
	char **al = articleList;
	int e, ef = 0, i, j = sizeof(articleList) / sizeof(articleList[0]);

	chdir(getenv("srcdir"));
	if (argc > 1) {
		al = argv + 1;
		j = argc - 1;
	}

	cout << "cArticle:\n";
	for (i = 0; i < j; i++) {
		if ((e = checkArticle(al[i])) < 0) {
			cout << "checkArticle(" << al[i] <<
			    ") failed with: " << e << endl;
			ef++;
		} else {
			cout << "checkArticle(" << al[i] <<
			    ") succeeded\n";
		}
	}
	if (ef == 0) {
		cout << "cArticle: all tests succeeded\n";
		exit(0);
	} else {
		cout << "cArticle: " << ef << " failures\n";
		exit(-1);
	}
}
