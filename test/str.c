#include <string.h>

int main (void)
{
	char *s = "This is a testing string. All work and no play makes Jack a dull boy. Lorem ipsum dolor sit amet, consectetur adipiscing elit";
	char buf[1024];
	int a = 2;

	char *p = strstr (s, "dull");
	if (p)
		a = 3;

	a = strlen (s);
	a = 2;

	a = strncmp ("test", s, 4);
	a = strcmp ("test", "test");
	if (a)
		a = 0;

	strncpy (buf, "strncpy", 7);
	strcat (buf, "strcat");
	strncat (buf, "strncat", 7);
	strcpy (buf, "strcpy");

        return 0;
}

