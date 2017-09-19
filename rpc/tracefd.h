#ifndef __RETRACE_TRACEFD_H__
#define __RETRACE_TRACEFD_H__

enum tracefd_keytype {
	tracefd_keytype_fd,
	tracefd_keytype_dirp,
	tracefd_keytype_stream
};

void init_tracefd_handlers(struct retrace_handle *handle);

struct fdinfo {
	SLIST_ENTRY(fdinfo) next;
	enum tracefd_keytype type;
	pid_t pid;
	void *key;
	char *info;
};

SLIST_HEAD(fdinfo_h, fdinfo);

void set_fdinfo(struct fdinfo_h *infos, pid_t pid, int fd, const char *info);
const struct fdinfo *get_fdinfo(struct fdinfo_h *infos, pid_t pid, int fd);

void set_dirinfo(struct fdinfo_h *infos, pid_t pid, DIR *dir,
	const char *info);
const struct fdinfo *get_dirinfo(struct fdinfo_h *infos, pid_t pid,
	DIR *dir);

void set_streaminfo(struct fdinfo_h *infos, pid_t pid, FILE *stream,
	const char *info);
const struct fdinfo *get_streaminfo(struct fdinfo_h *infos, pid_t pid,
	FILE *stream);
#endif
