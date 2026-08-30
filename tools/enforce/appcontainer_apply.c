/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "appcontainer_apply.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>

/*
 * Grant one path to the container SID: (GR,FX) for reads,
 * (GW,GR,FX) for writes. Failures are reported but never
 * fatal at grant time -- the container's deny-by-default
 * stays the floor; a failed grant merely narrows it.
 */
static void grant_path(PSID sid, const char *path, int write)
{
	PACL old = NULL, acl = NULL;
	PSECURITY_DESCRIPTOR sd = NULL;
	EXPLICIT_ACCESS_A ea;
	DWORD rc;

	rc = GetNamedSecurityInfoA(path, SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION, NULL, NULL, &old, NULL, &sd);
	if (rc != ERROR_SUCCESS)
		return;
	memset(&ea, 0, sizeof(ea));
	ea.grfAccessPermissions = GENERIC_EXECUTE | GENERIC_READ |
		(write ? GENERIC_WRITE : 0);
	ea.grfAccessMode = GRANT_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	ea.Trustee.ptstrName = (LPSTR)sid;
	if (SetEntriesInAclA(1, &ea, old, &acl) == ERROR_SUCCESS)
		(void)SetNamedSecurityInfoA((LPSTR)path,
			SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
			NULL, NULL, acl, NULL);
	if (sd != NULL)
		LocalFree(sd);
	if (acl != NULL)
		LocalFree(acl);
}

int enforce_appcontainer_apply(const struct enforce_spec *spec,
	char *const argv[], char *const envp[])
{
	PSID sid = NULL;
	STARTUPINFOEXA si;
	PROCESS_INFORMATION pi;
	SIZE_T attr_sz = 0;
	wchar_t wname[256], wdisp[32], wdesc[128];
	char cmdline[32768];
	size_t o = 0;
	int i;
	DWORD rc;

	if (spec->ac_name[0] == '\0' || argv == NULL ||
	    argv[0] == NULL)
		return -1;

	/* the profile APIs are W-only: name/display/description
	 * ride UTF-16; the narrow spec name converts here once
	 */
	if (MultiByteToWideChar(CP_UTF8, 0, spec->ac_name, -1, wname,
		    (int)(sizeof(wname) / sizeof(wname[0]))) == 0)
		return -1;
	MultiByteToWideChar(CP_UTF8, 0, "retrace", -1, wdisp,
		(int)(sizeof(wdisp) / sizeof(wdisp[0])));
	MultiByteToWideChar(CP_UTF8, 0,
		"retrace enforcement container", -1, wdesc,
		(int)(sizeof(wdesc) / sizeof(wdesc[0])));

	/*
	 * create once; reuse when the profile already exists. The
	 * toolchain headers disagree on this API's arity (MinGW's
	 * userenv.h drops the capabilities pointer), so resolve the
	 * documented seven-argument shape from userenv.dll -- the
	 * ABI is the truth both headers gesture at.
	 */
	{
		typedef HRESULT WINAPI create_profile_t(PCWSTR,
			PCWSTR, PCWSTR, PSID, DWORD,
			const SECURITY_CAPABILITIES *, PSID *);
		HMODULE uv = GetModuleHandleA("userenv.dll");
		create_profile_t *create_profile =
			uv != NULL ? (create_profile_t *)GetProcAddress(
				uv, "CreateAppContainerProfile") : NULL;

		if (create_profile == NULL ||
		    create_profile(wname, wdisp, wdesc, NULL, 0, NULL,
			    &sid) != S_OK) {
			HRESULT hr =
				DeriveAppContainerSidFromAppContainerName(
					wname, &sid);

			if (hr != S_OK || sid == NULL) {
				fprintf(stderr,
					"retrace-enforce: appcontainer derive failed\n");
				return -1;
			}
		}
	}

	for (i = 0; i < (int)spec->ac_read_n; i++)
		grant_path(sid, spec->ac_read[i], 0);
	for (i = 0; i < (int)spec->ac_write_n; i++)
		grant_path(sid, spec->ac_write[i], 1);

	/* rebuild the command line from argv */
	for (i = 0; argv[i] != NULL; i++) {
		int n = snprintf(cmdline + o, sizeof(cmdline) - o,
			"%s\"%s\"", i > 0 ? " " : "", argv[i]);

		if (n <= 0 || (size_t)n >= sizeof(cmdline) - o) {
			fprintf(stderr,
				"retrace-enforce: command line overflow\n");
			LocalFree(sid);
			return -1;
		}
		o += (size_t)n;
	}

	memset(&si, 0, sizeof(si));
	si.StartupInfo.cb = sizeof(si);
	InitializeProcThreadAttributeList(NULL, 1, 0, &attr_sz);
	si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)
		HeapAlloc(GetProcessHeap(), 0, attr_sz);
	if (si.lpAttributeList == NULL ||
	    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0,
		    &attr_sz) != TRUE ||
	    UpdateProcThreadAttribute(si.lpAttributeList, 0,
		    PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
		    sid, GetLengthSid(sid), NULL, NULL) != TRUE) {
		fprintf(stderr,
			"retrace-enforce: attribute list failed\n");
		if (si.lpAttributeList != NULL)
			HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
		LocalFree(sid);
		return -1;
	}
	memset(&pi, 0, sizeof(pi));
	/* the env block is narrow char ** -- CREATE_UNICODE_ENVIRONMENT
	 * would tell the child to read it as UTF-16
	 */
	if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
		    EXTENDED_STARTUPINFO_PRESENT,
		    envp != NULL ? (LPVOID)envp : NULL, NULL,
		    &si.StartupInfo, &pi) != TRUE) {
		fprintf(stderr,
			"retrace-enforce: CreateProcess in container failed: %lu\n",
			(unsigned long)GetLastError());
		DeleteProcThreadAttributeList(si.lpAttributeList);
		HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
		LocalFree(sid);
		return -1;
	}
	DeleteProcThreadAttributeList(si.lpAttributeList);
	HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
	LocalFree(sid);

	WaitForSingleObject(pi.hProcess, INFINITE);
	if (!GetExitCodeProcess(pi.hProcess, &rc))
		rc = (DWORD)-1;
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return (int)rc;
}

#else /* !_WIN32 */

int enforce_appcontainer_apply(const struct enforce_spec *spec,
	char *const argv[], char *const envp[])
{
	(void)spec;
	(void)argv;
	(void)envp;
	return 1;	/* the plane exists only on Windows */
}

#endif /* _WIN32 */
