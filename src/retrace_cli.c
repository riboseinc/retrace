/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* because of _XOPEN_SOURCE, TODO: cross platform */
#if __linux__

#define  _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "retrace_cli.h"

#define CLI_MAX_MENU_ITEMS 20
#define CLI_IN_BUFF_SIZE 128
#define CLI_OUT_BUFF_SIZE CLI_IN_BUFF_SIZE

typedef struct
{
	unsigned int id;
	cli_cmd_t cmd;
} cli_menu_item_t;

static int pts_fd = -1;
static cli_menu_item_t menu_items[CLI_MAX_MENU_ITEMS];
static unsigned int menu_cnt;
static pthread_mutex_t api_mutex;
static pthread_mutex_t data_mutex;

int cli_init(char *pts_path, size_t path_len)
{
	int res;
	char *pts;
	struct termios tios;

	/* initialize api mutex */
	res = pthread_mutex_init(&api_mutex, NULL);
	if (res < 0)
		return res;

	/* initialize data mutex */
	res = pthread_mutex_init(&data_mutex, NULL);
	if (res < 0)
		return res;

	/* open and unlock pts */
	pts_fd = posix_openpt(O_RDWR | O_NONBLOCK);
	if (pts_fd < 0)
		return -1;

	if (grantpt(pts_fd) < 0) {
		close(pts_fd);
		return -1;
	}

	if (unlockpt(pts_fd) < 0) {
		close(pts_fd);
		return -1;
	}

	/* set raw mode */
	if (tcgetattr(pts_fd, &tios)) {
		close(pts_fd);
		return -1;
	}

	cfmakeraw(&tios);

	if (tcsetattr(pts_fd, TCSANOW, &tios)) {
		close(pts_fd);
		return -1;
	}

	/* return pts name */
	pts = ptsname(pts_fd);
	if (pts == NULL) {
		close(pts_fd);
		return -1;
	}

	if ((strlen(pts) + 1) > path_len) {
		close(pts_fd);
		return -1;
	}

	strcpy(pts_path, pts);

	return 0;
}

int cli_printf(const char *format, ...)
{
	/* 1 for '\0' */
	static char out_buff[CLI_OUT_BUFF_SIZE + 1];
	int sz;
	va_list args;

	if (pts_fd == -1)
		return -1;

	pthread_mutex_lock(&api_mutex);

	va_start(args, format);
	sz = vsnprintf(out_buff, sizeof(out_buff), format, args);
	va_end(args);

	sz = write(pts_fd, out_buff, sz);

	/* not sure if needed */
	tcdrain(pts_fd);

	pthread_mutex_unlock(&api_mutex);

	return sz;
}

int cli_scanf(const char *format, ...)
{
	static char in_buff[CLI_IN_BUFF_SIZE + 1];
	fd_set sel_set;
	char in;
	unsigned int idx;
	va_list args;
	int res;

	if (pts_fd == -1)
		return -1;

	pthread_mutex_lock(&api_mutex);

	tcflush(pts_fd, TCIFLUSH);

	idx = 0;
	in = 0;

	/* read char by char till no more space or Main Enter key is pressed */
	while ((idx != CLI_IN_BUFF_SIZE) && (in != '\n')) {

		FD_ZERO(&sel_set);
		FD_SET(pts_fd, &sel_set);

		if (select(pts_fd + 1, &sel_set, NULL, NULL, NULL) == -1)
			return EOF;

		if (!FD_ISSET(pts_fd, &sel_set))
			continue;

		/* read next character */
		if (read(pts_fd, &in, 1) !=  1)
			return EOF;

		/* echo back */
		if (write(pts_fd, &in, 1) == -1)
			return EOF;

		/* currently support only main Enter key as end of input */
		if (in == '\r') {
			in = '\n';
			if (write(pts_fd, &in, 1) == -1) {
				return EOF;
			}
		}
		in_buff[idx++] = in;
	}

	/* indicate error in case no more space in buffer */
	if (in != '\n')
		return EOF;

	in_buff[idx] = '\0';
	va_start(args, format);
	res = vsscanf(in_buff, format, args);
	va_end(args);

	pthread_mutex_unlock(&api_mutex);
	return res;
}

void cli_run(void)
{
	unsigned int i;
	unsigned int cmd_id;

	if (pts_fd == -1)
		return;

	while (1) {

		cli_printf("Retrace command menu\r\n");
		cli_printf("--------------------\r\n");

		/*
		 * print command menu
		 * menu contents can be modified by cli_register_command_blk()
		 */
		pthread_mutex_lock(&data_mutex);

		i = 0;
		while (i != menu_cnt) {
			cli_printf("[%u] %s\r\n", menu_items[i].id, menu_items[i].cmd.name);
			i++;
		}

		pthread_mutex_unlock(&data_mutex);

		/* read command id */
		cli_printf("Enter command id>> ");

		if (cli_scanf("%u", &cmd_id) != 1) {
			cli_printf("Failed to get command id, retry\r\n");
			continue;
		}

		if (cmd_id >= menu_cnt) {
			cli_printf("Invalid command id: %u\r\n", cmd_id);
		} else {
			/* Dispatch command */
			cli_printf("command id %u START \r\n", cmd_id);
			menu_items[cmd_id].cmd.func();
			cli_printf("command id %u DONE \r\n", cmd_id);
		}
	}
}

int cli_register_command_blk(const cli_cmd_t *cmd_blk)
{
	const cli_cmd_t *p;

	p = cmd_blk;

	/* Add block to the end of menu_items */
	pthread_mutex_lock(&data_mutex);

	while ((menu_cnt != CLI_MAX_MENU_ITEMS) && p->func) {
		menu_items[menu_cnt].id = menu_cnt;
		menu_items[menu_cnt].cmd = *p;

		menu_cnt++;
		p++;
	}

	pthread_mutex_unlock(&data_mutex);

	if (p->func) {
		/* no more space */
		return -1;
	}

	return 0;
}

#endif /* __linux__ */

