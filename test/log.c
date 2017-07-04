#include <syslog.h>
#include <stdarg.h>

void more_tests(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
}

int main(void)
{
	openlog("retrace_log_test", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_USER);

	setlogmask(LOG_MASK(LOG_INFO) | LOG_MASK(LOG_ERR));

	syslog(LOG_INFO, "Test log %s %d", "string", 42);

	closelog();

	more_tests("vsyslog %d\n", 42);


	return 0;
}

