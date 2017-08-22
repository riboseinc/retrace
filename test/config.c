#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void)
{
	int (*rtr_get_config_single)(const char *key, ...);
	int (*rtr_get_config_multiple)(RTR_CONFIG_HANDLE *ch, const char *key, ...);
	int r, i, j;
	char *s;
	double n;
	RTR_CONFIG_HANDLE ch;

	rtr_get_config_single = dlsym(RTLD_DEFAULT, "rtr_get_config_single");
	rtr_get_config_multiple = dlsym(RTLD_DEFAULT, "rtr_get_config_multiple");

	r = rtr_get_config_single("config-test", ARGUMENT_TYPE_END);
	assert(r != 0);

	r = rtr_get_config_single("config-test", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END, &s);
	assert(r == 0);

	r = rtr_get_config_single("config-test1", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END, &s, &i, &n);
	assert(r != 0);
	assert(!strcmp(s, "five"));
	assert(i == 6);
	assert(n == 7.0);

	ch = RTR_CONFIG_START;
	j = 0;
	do {
		assert(j < 4);
		++j;
		r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
		    ARGUMENT_TYPE_END, &s, &i);
	} while (r);

	ch = RTR_CONFIG_START;
	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	assert(r != 0);
	assert(!strcmp(s, "forty"));
	assert(i == 2);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	assert(r != 0);
	assert(!strcmp(s, "ninety"));
	assert(i == 9);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	assert(r != 0);
	assert(!strcmp(s, "seventy"));
	assert(i == 6);

	r = rtr_get_config_multiple(&ch, "config-test2", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT,
	    ARGUMENT_TYPE_END, &s, &i);
	assert(r == 0);

	return 0;
}
