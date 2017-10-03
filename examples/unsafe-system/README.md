# Detect unsafe use of system()
Retrace can be used to find usafe usage of `system()` calls

## How is system() unsafe?
Setuid binaries that use `system()` are susceptible to PATH attacks if the shell commands used are relative.

Example of unsafe use:

`system.c`:
```C
#include <unistd.h>
#include <stdlib.h>

int
main()
{
	setuid(0);
	system("id");
	return(0);
}
```

```console
$ gcc system.c -o system
$ sudo chown root:wheel system
$ sudo chmod 4755 system
$ ./system
uid=0(root) gid=0(wheel) egid=20(staff) groups=0(wheel)
$
```

Now exploit this unsafe use of `system()`:

```console
$ echo "echo OWNED" > ./id
$ chmod +x ./id
$ export PATH=.
$ ./system
OWNED
$
```

## Using Retrace to identify a system() vulnerability

Retrace will show any unsafe use of `system()`:

```console
$ ../../retrace ./system
<SNIP>
uid=501(test) gid=20(staff) groups=20(staff)
(11556) system("id") = 0 [WARNING: might be using a relative path]
(11556) exit(0)
(11556) fwrite(0x7fff506805a8, 1, 0, 0x7fffeb3bf1a8 [fd 1]) = 0
(11556) fwrite(0x7fff506805a8, 1, 0, 0x7fffeb3bf1a8 [fd 1]) = 0
(11556) fwrite(0x7fff506805a8, 1, 0, 0x7fffeb3bf240 [fd 2]) = 0
(11556) fwrite(0x7fff506805a8, 1, 0, 0x7fffeb3bf240 [fd 2]) = 0
(11556) _exit(0)
$
```

Any **safe** use of `system()` will not show that alert:

```console
$ cat system.c
#include <unistd.h>
#include <stdlib.h>

int
main()
{
	setuid(0);
	system("/usr/bin/id");
	return(0);
}
$ gcc system.c -o system
$ ../../retrace ./system
<SNIP>
uid=501(test) gid=20(staff) groups=20(staff)
(11697) system("/usr/bin/id", ) = 0
(11697) exit(0, )
(11697) fwrite(0x7fff50cc35c8, 1, 0, 0x7fffeb3bf1a8 [fd 1], ) = 0
(11697) fwrite(0x7fff50cc35c8, 1, 0, 0x7fffeb3bf1a8 [fd 1], ) = 0
(11697) fwrite(0x7fff50cc35c8, 1, 0, 0x7fffeb3bf240 [fd 2], ) = 0
(11697) fwrite(0x7fff50cc35c8, 1, 0, 0x7fffeb3bf240 [fd 2], ) = 0
(11697) _exit(0, )
$
```
