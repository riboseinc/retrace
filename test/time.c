#include <time.h>

int main (void)
{
	char buf[26];

	time_t t = time(NULL);

	ctime(&t);
	ctime_r(&t, buf);

        return 0;
}

