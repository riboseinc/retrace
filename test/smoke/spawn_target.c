/*
 * The audited-launch E2E's workload (TODO.impl/11): alive long
 * enough for the injected agent to connect and HELLO through
 * the pipe (EAGER join), then exits with a KNOWN code the test
 * asserts in the journal's retrace.ctl.exit record.
 *
 * argv[1] (optional): a marker directory. The heartbeat below
 * is RAW Win32 (no CRT call, no dispatch): which markers exist
 * localizes a stuck process -- nothing (pre-main), ticks only
 * (mid-Sleep), ticks+exiting (stuck at exit).
 */

#include <windows.h>

static void mark(const char *dir, const char *name, int n)
{
	char path[MAX_PATH];
	HANDLE h;
	char body[32];
	DWORD written;

	if (dir == NULL || dir[0] == '\0')
		return;
	lstrcpyA(path, dir);
	lstrcatA(path, "\\");
	lstrcatA(path, name);
	h = CreateFileA(path, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return;
	written = 0;
	wsprintfA(body, "%d\n", n);
	WriteFile(h, body, lstrlenA(body), &written, NULL);
	CloseHandle(h);
}

int main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : NULL;
	int i;

	for (i = 0; i < 10; i++) {
		mark(dir, "tick.txt", i);
		Sleep(300);
	}
	mark(dir, "exiting.txt", 7);
	return 7;
}
