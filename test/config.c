#include "common.h"
#include <stdio.h>
#include <stdlib.h>

#define FAIL(exp)								\
	do {									\
		if (exp) {							\
			fprintf(stderr, "test failed at line %d", __LINE__);	\
			exit(1);						\
		}								\
	} while (0)

int main(void)
{
	int (*rtr_get_config_single)(const char *key, ...);
	int (*rtr_get_config_multiple)(RTR_CONFIG_HANDLE *ch, const char *key, ...);
	int r, i;
	char *s;
	double n;
	RTR_CONFIG_HANDLE ch;

	rtr_get_config_single = dlsym(RTLD_DEFAULT, "rtr_get_config_single");
	rtr_get_config_multiple = dlsym(RTLD_DEFAULT, "rtr_get_config_multiple");

	r = rtr_get_config_single("config-test", ARGUMENT_TYPE_END);
	FAIL(r == 0);

	r = rtr_get_config_single("config-test", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END, &s);
	FAIL(r != 0);

	r = rtr_get_config_single("config-test1", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &s, &i, &n);
	FAIL(r == 0);
	FAIL(strcmp(s, "five"));
	FAIL(i != 6);
	FAIL(n != 7.0);

	ch = RTR_CONFIG_START;
	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	FAIL(r == 0);
	FAIL(strcmp(s, "forty"));
	FAIL(i != 2);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	FAIL(r == 0);
	FAIL(strcmp(s, "ninety"));
	FAIL(i != 9);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	FAIL(r == 0);
	FAIL(strcmp(s, "seventy"));
	FAIL(i != 6);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	FAIL(r != 0);
}
