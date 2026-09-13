/*
 * The audited-launch E2E's workload (TODO.impl/11): alive long
 * enough for the injected agent to connect and HELLO through
 * the pipe (EAGER join), then exits with a KNOWN code the test
 * asserts in the journal's retrace.ctl.exit record.
 */

#include <windows.h>

int main(void)
{
	Sleep(3000);
	return 7;
}
