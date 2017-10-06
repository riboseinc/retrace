#ifndef __RETRACE_STRINJECT_H__
#define __RETRACE_STRINJECT_H__

/*
 * retrace string injection types
 */

enum RTR_STRINJECT_TYPE {
	STRINJECT_TYPE_HEX = 0,
	STRINJECT_TYPE_FMT_STR,
	STRINJECT_TYPE_BUF_OVERFLOW,
	STRINJECT_TYPE_FILE_LINE,
	STRINJECT_TYPE_STANDARD
};

/*
 * retrace string inject function IDs
 */

enum RTR_STRINJECT_FUNC_ID {
	STRINJECT_FUNC_FWRITE = 0,		/* fwrite() */
	STRINJECT_FUNC_FREAD,			/* fread() */
	STRINJECT_FUNC_READ,			/* read() */
	STRINJECT_FUNC_WRITE,			/* write() */
	STRINJECT_FUNC_SEND,			/* send() */
	STRINJECT_FUNC_SENDTO,			/* sendto() */
	STRINJECT_FUNC_SENDMSG,			/* sendmsg() */
	STRINJECT_FUNC_RECV,			/* recv() */
	STRINJECT_FUNC_RECVFROM,		/* recvfrom() */
	STRINJECT_FUNC_RECVMSG,			/* recvmsg() */
	STRINJECT_FUNC_SSL_READ,		/* SSL_read() */
	STRINJECT_FUNC_SSL_WRITE,		/* SSL_write() */
	STRINJECT_FUNC_MAX
};

/*
 * retrace string injection configuration
 */

struct rtr_strinject_info {
	int exist;

	enum RTR_STRINJECT_TYPE type;
	const char *param;
	const char *param2;
	double rate;
};

/*
 * string injection function
 */

int rtr_str_inject(enum RTR_STRINJECT_FUNC_ID func_id, const void *buffer, size_t len, void **inject_buffer, size_t *inject_len);

#endif /* __RETRACE_STRINJECT_H__ */
