/* this is sample code to demonstrate how retrace can be used to root checking protection mechanisms
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	uid_t uid, euid;

	uid = getuid();
	euid = geteuid();

	/* ensure this always get run as root */
	if (uid > 0 || uid != euid) {
		printf("need full root privileges (uid=%d euid=%d)\n", uid, euid);
		exit(1);
	}

	printf("got full root! uid=%d euid=%d\n", uid, euid);

	/* do proprietary stuff here */

	return(0);
}
