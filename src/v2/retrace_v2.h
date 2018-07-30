
#ifndef __RETRACE_V2_H__
#define __RETRACE_V2_H__

#ifndef __APPLE__
#define RTR2_LIB_NAME             "libretrace_v2.so"
#else
#define RTR2_LIB_NAME             "libretrace_v2.0.dylib"
#endif

#define MAX_PATH                      256

/*
 * retrace V2 options
 */

struct rtr2_options {
	char *config_path;

	char *proc_cli;

	char *lib_path;
	char *sys_lib_path;

	char *log_path;
	int verbose;

	bool print_version;

	/* these options should be free after used */
	char *bin_path;
	char **bin_args;
	int bin_args_count;
};

#endif /* __RETRACE_V2_H__ */
