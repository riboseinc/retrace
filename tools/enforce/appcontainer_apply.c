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
#define _WIN32_WINNT 0x0601
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
	SID_AND_ATTRIBUTES caps[1];
	STARTUPINFOEXA si;
	PROCESS_INFORMATION pi;
	SIZE_T attr_sz = 0;
	char cmdline[32768];
	size_t o = 0;
	int i;
	DWORD rc;

	if (spec->ac_name[0] == '\0' || argv == NULL ||
	    argv[0] == NULL)
		return -1;

	/* create once; reuse when the profile already exists */
	if (CreateAppContainerProfile(spec->ac_name, "retrace",
		"retrace enforcement container", NULL, NULL,
		&sid) != S_OK) {
		HRESULT hr = DeriveAppContainerSidFromAppContainerName(
			spec->ac_name, &sid);

		if (hr != S_OK || sid == NULL) {
			fprintf(stderr,
				"retrace-enforce: appcontainer derive failed\n");
			return -1;
		}
	}

	/* no capabilities: least privilege (no net, no devices) */
	memset(caps, 0, sizeof(caps));

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
	si.AttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)
		HeapAlloc(GetProcessHeap(), 0, attr_sz);
	if (si.AttributeList == NULL ||
	    InitializeProcThreadAttributeList(si.AttributeList, 1, 0,
		    &attr_sz) != TRUE ||
	    UpdateProcThreadAttribute(si.AttributeList, 0,
		    PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
		    sid, GetLengthSid(sid), NULL, NULL) != TRUE) {
		fprintf(stderr,
			"retrace-enforce: attribute list failed\n");
		if (si.AttributeList != NULL)
			HeapFree(GetProcessHeap(), 0, si.AttributeList);
		LocalFree(sid);
		return -1;
	}
	memset(&pi, 0, sizeof(pi));
	if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
		    EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
		    envp != NULL ? (LPVOID)envp : NULL, NULL,
		    &si.StartupInfo, &pi) != TRUE) {
		fprintf(stderr,
			"retrace-enforce: CreateProcess in container failed: %lu\n",
			(unsigned long)GetLastError());
		DeleteProcThreadAttributeList(si.AttributeList);
		HeapFree(GetProcessHeap(), 0, si.AttributeList);
		LocalFree(sid);
		return -1;
	}
	DeleteProcThreadAttributeList(si.AttributeList);
	HeapFree(GetProcessHeap(), 0, si.AttributeList);
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
