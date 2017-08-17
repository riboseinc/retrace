#include <sys/types.h>
#include <sys/ptrace.h>

int main(void)
{
	ptrace(PT_TRACE_ME, 42, (caddr_t) 0xdeadbeef, 42);

	return 0;
}
