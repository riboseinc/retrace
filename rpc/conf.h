#ifndef __CONF_H__
#define __CONF_H__

#include <sys/queue.h>

struct config_entry {
	STAILQ_ENTRY(config_entry) next;
	char *key;
	char *value;
};

STAILQ_HEAD(config, config_entry);

int read_config_file(struct config *config, const char *filename);
int add_config_entry(struct config *config, const char *key,
	const char *value);
void split_config(struct config *c1, struct config *c2, ...);
void free_config_entries(struct config *c);

#endif
