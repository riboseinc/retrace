/*
 * This code should loop forever without tracing ever
 * being disabled
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

void *thread_function(void *ptr);

int main(void)
{
	pthread_t thread1, thread2;

	pthread_create(&thread1, NULL, thread_function, (void *)NULL);
	pthread_create(&thread2, NULL, thread_function, (void *)NULL);

	while (1)
		getuid();

	return 0;
}

void *thread_function(void *ptr)
{
	while (1)
		getuid();
}
