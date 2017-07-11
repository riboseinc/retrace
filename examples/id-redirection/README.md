# Retrace getuid() redirection
Retrace can be used to redirect `getuid()` and `geteuid()` calls to return a different UID than the user that is running the executable.

When reverse engineering proprietary executables of unknown origin it might be of benefit to defeat root checks such as this:
```
uid_t uid, euid;

uid = getuid();
euid = geteuid();

if (uid > 0 || uid != euid) {
	printf("need full root privileges (uid=%d euid=%d)\n", uid, euid);
	exit(1);
}
```

## Retrace configuration syntax:

Always make `getuid()` return 0:
```
getuid,0
```

Always make `geteuid()` return 0:
```
geteuid,0
```

Make both `getuid()` and `geteuid()` return 0:
```
geteuid,0
getuid,0
```

## Example run:
Show `getuid()` bypass in `examples/id-redirection/root-check`:
```sh
$ cat examples/id-redirection/get-both-redirect.conf
getuid,0
geteuid,0
$ ./retrace -f examples/id-redirection/get-both-redirect.conf examples/id-redirection/root-check
<SNIP>
(44893) getuid() = 0 [redirection in effect: '0']
(44893) geteuid() = 0 [redirection in effect: '0']
(44893) printf("got full root! uid=%d euid=%d\n" -> "got full root! uid=0 euid=0\n", ) = 28
got full root! uid=0 euid=0
$ 
```

To show `getuid()` redirection in action on macOS (copy `id` to `/tmp` to bypass SIP):
```sh
$ whereis id
/usr/bin/id
$ /usr/bin/id
uid=501(test) gid=20(staff) groups=20(staff),401(com.apple.sharepoint.group.1),12(everyone),61(localaccounts),79(_appserverusr),80(admin),81(_appserveradm),98(_lpadmin),501(access_bpf),701(com.apple.sharepoint.group.3),33(_appstore),100(_lpoperator),204(_developer),395(com.apple.access_ftp),398(com.apple.access_screensharing),399(com.apple.access_ssh),402(com.apple.sharepoint.group.2)
$ cp /usr/bin/id /tmp/id
$ cat examples/id-redirection/get-l33t-redirect.conf
getuid,1337
geteuid,1337
$ ./retrace -f examples/id-redirection/get-l33t-redirect.conf /tmp/id
<SNIP>
(45047) getuid() = 1337 [redirection in effect: '1337']
(45047) geteuid() = 1337 [redirection in effect: '1337']
(45047) getuid() = 1337 [redirection in effect: '1337']
(45047) geteuid() = 1337 [redirection in effect: '1337']
(45047) getuid() = 1337 [redirection in effect: '1337']
(45047) getuid() = 1337 [redirection in effect: '1337']
(45047) geteuid() = 1337 [redirection in effect: '1337']
uid=1337 gid=20(staff) groups=20(staff),401(com.apple.sharepoint.group.1),12(everyone),61(localaccounts),79(_appserverusr),80(admin),81(_appserveradm),98(_lpadmin),501(access_bpf),701(com.apple.sharepoint.group.3),33(_appstore),100(_lpoperator),204(_developer),395(com.apple.access_ftp),398(com.apple.access_screensharing),399(com.apple.access_ssh)
$ 
```

## Try it out yourself!
Use `test-id-redirection.sh` or experiment with different configuration and test executables.
