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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>

#define MAX_BUF_LEN					2048
#define MAX_POSSTR_LEN					sizeof(off_t) * 8 - 1
#define MAX_LINE_LEN					1024

#define MAX_FILE_NUM_TYPE_1				256

#define FORMAT_STR					"%s"
#define INJECT_STR_CHAR					'\x41'

/* inject types */
enum STR_INJECT_TYPE {
	STR_INJECT_TYPE_1,
	STR_INJECT_TYPE_2,
	STR_INJECT_TYPE_3,
	STR_INJECT_TYPE_4,
	STR_INJECT_TYPE_5,
	STR_INJECT_TYPE_UNKNOWN,
};

/*
 * print help
 */

static void print_help(void)
{
	fprintf(stderr, "Usage: stringinjector [options]\n"
		"[options]:\n"
		"\tType1: [input file] [output file template] -h [position]\n"
		"\tType2: [input file] [output file template] -h [position1,position2]\n"
		"\tType3: [input file] [output file] -f [position] [count] [-r]\n"
		"\tType4: [input file] [output file] -o [position] [len] [-r]\n"
		"\tType5: [input file] [output file template] -i [position] [file which has inject lines] [-r]\n");

	exit(1);
}

/*
 * get file size
 */

static off_t get_fsize(const char *fpath)
{
	struct stat st;

	/* get stat of file */
	if (stat(fpath, &st) != 0)
		return -1;

	return st.st_size;
}

/*
 * parse position param for type 1
 */

static int parse_position_param(const char *pos_str, const char *fpath, off_t *_pos)
{
	off_t pos;

	/* check position */
	pos = strtol(pos_str, NULL, 10);
	if (pos < 0 || pos == LONG_MAX) {
		fprintf(stderr, "Invalid position '%s'\n", pos_str);
		return -1;
	}

	if (pos > get_fsize(fpath)) {
		fprintf(stderr, "File size is too small to inject.\n");
		return -1;
	}

	*_pos = pos;

	return 0;
}

/*
 * parse position param for type 2
 */
static int parse_position_param2(const char *pos_str, const char *fpath, off_t *_pos1, off_t *_pos2)
{
	char pos1_str[MAX_POSSTR_LEN + 1], pos2_str[MAX_POSSTR_LEN + 1];
	char *p;

	off_t pos1, pos2, tmp_pos;
	off_t fsize;

	/* check ',' character */
	p = strchr(pos_str, ',');
	if (!p || p == pos_str || strlen(p) == 1) {
		fprintf(stderr, "Invalid position string(%s). Should be 'pos1,pos2' format\n", pos_str);
		return -1;
	}

	if (p - pos_str > MAX_POSSTR_LEN || strlen(p) > MAX_POSSTR_LEN) {
		fprintf(stderr, "Invalid position string(%s). Too long position\n", pos_str);
		return -1;
	}

	memset(pos1_str, 0, sizeof(pos1_str));
	memset(pos2_str, 0, sizeof(pos2_str));

	strncpy(pos1_str, pos_str, p - pos_str);
	strncpy(pos2_str, p + 1, strlen(p));

	/* check position */
	pos1 = strtol(pos1_str, NULL, 10);
	pos2 = strtol(pos2_str, NULL, 10);
	if (pos1 < 0 || pos2 < 0 || pos1 == pos2) {
		fprintf(stderr, "Invalid position '%s'\n", pos_str);
		return -1;
	}

	fsize = get_fsize(fpath);
	if (pos1 > fsize || pos2 > fsize) {
		fprintf(stderr, "File size is too small to inject.\n");
		return -1;
	}

	/* revert position value if first is bigger than second */
	if (pos1 > pos2) {
		tmp_pos = pos1;
		pos1 = pos2;
		pos2 = tmp_pos;
	}

	/* set result */
	*_pos1 = pos1;
	*_pos2 = pos2;

	return 0;
}

/*
 * open file for reading
 */

static FILE *open_file(const char *fpath, int read_only)
{
	int fd;
	FILE *fp;

	/* open file */
	if (read_only)
		fd = open(fpath, O_RDONLY);
	else
		fd = open(fpath, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);

	if (fd < 0)
		return NULL;

	return fdopen(fd, read_only ? "rb" : "wb");
}

/*
 * write buffer into destination file
 */

static void write_dst_file(FILE *fp, void *buffer, size_t size)
{
	int i;
	size_t write_total = 0;

	/* check write size */
	if (size == 0)
		return;

	/* write buffer */
	while (write_total < size) {
		size_t write_bytes;

		write_bytes = fwrite((char *) buffer + write_total, 1, size - write_total, fp);
		if (write_bytes < 0)
			break;

		write_total += write_bytes;
	}
}

/*
 * inject single hex
 */

static void inject_single_hex(FILE *fp, void *buffer, size_t buffer_len, off_t offset, char inject)
{
	/* write offset bytes */
	if (offset > 0)
		write_dst_file(fp, buffer, offset);

	/* inject hex value */
	write_dst_file(fp, &inject, 1);

	/* write remain bytes */
	if (buffer_len - offset - 1 > 0)
		write_dst_file(fp, buffer + offset + 1, buffer_len - offset - 1);
}

/*
 * inject multiple hex values
 */

static void inject_multiple_hex(FILE *src_fp, FILE *dst_fp, void *buffer, size_t read_total, size_t read_len,
	void *inject, size_t inject_len, off_t pos, int replace)
{
	size_t offset = read_len - (read_total - pos);

	/* write offset bytes */
	if (offset > 0)
		write_dst_file(dst_fp, buffer, offset);

	/* write inject buffer */
	write_dst_file(dst_fp, inject, inject_len);

	/* write remain data */
	if (replace) {
		if (read_len - offset > inject_len)
			write_dst_file(dst_fp, buffer + offset + inject_len, read_len - offset - inject_len);
		else {
			/* seek file position */
			fseek(src_fp, inject_len - (read_total - pos), SEEK_CUR);
		}
	} else
		write_dst_file(dst_fp, buffer + offset, read_len - offset);
}

/*
 * single hex byte injection
 */

static void str_inject_func_t1(const char *src_fpath, const char *dst_fpath_templ, const char *pos_str)
{
	FILE *fp;
	off_t pos;

	int i;

	/* parse position param */
	if (parse_position_param(pos_str, src_fpath, &pos) != 0)
		return;

	/* open source file */
	fp = open_file(src_fpath, 1);
	if (!fp) {
		fprintf(stderr, "Couln't open file '%s'\n", src_fpath);
		return;
	}

	/* open destination file */
	for (i = 0; i < MAX_FILE_NUM_TYPE_1; i++) {
		FILE *dst_fp;
		char dst_fpath[512];

		size_t read_total = 0;
		int inject_completed = 0;

		/* open destination file */
		snprintf(dst_fpath, sizeof(dst_fpath), "%s.%03d", dst_fpath_templ, i);
		dst_fp = open_file(dst_fpath, 0);
		if (!dst_fp)
			continue;

		while (!feof(fp)) {
			size_t read_bytes;
			char buffer[MAX_BUF_LEN];

			/* read buffer from file */
			read_bytes = fread(buffer, 1, MAX_BUF_LEN, fp);

			read_total += read_bytes;
			if (read_total <= pos || inject_completed) {
				write_dst_file(dst_fp, buffer, read_bytes);
			} else {
				off_t offset = read_bytes - (read_total - pos);

				/* inject single hex value */
				inject_single_hex(dst_fp, buffer, read_bytes, offset, i);

				/* set flag for inject completion */
				inject_completed = 1;
			}

		}

		/* seek file position into begin */
		fseek(fp, 0L, SEEK_SET);

		/* close destination file */
		fclose(dst_fp);
	}

	/* close source file */
	fclose(fp);
}

/*
 * double hex bytes injection
 */

static void str_inject_func_t2(const char *src_fpath, const char *dst_fpath_templ, const char *pos_str)
{
	FILE *fp;
	off_t pos1, pos2;

	int i;

	/* parse positions */
	if (parse_position_param2(pos_str, src_fpath, &pos1, &pos2) < 0)
		return;

	/* open source file */
	fp = open_file(src_fpath, 1);
	if (!fp) {
		fprintf(stderr, "Couln't open file '%s'\n", src_fpath);
		return;
	}

	for (i = 0; i < MAX_FILE_NUM_TYPE_1; i++) {
		char dst_fpath[512];
		int j;

		for (j = 0; j < MAX_FILE_NUM_TYPE_1; j++) {
			FILE *dst_fp;
			int inject_completed1 = 0, inject_completed2 = 0;

			size_t read_total = 0;

			/* open destination file */
			snprintf(dst_fpath, sizeof(dst_fpath), "%s.%03d.%03d", dst_fpath_templ, i, j);
			dst_fp = open_file(dst_fpath, 0);
			if (!dst_fp)
				continue;

			while (!feof(fp)) {
				size_t read_bytes;
				char buffer[MAX_BUF_LEN];

				/* read buffer from file */
				read_bytes = fread(buffer, 1, MAX_BUF_LEN, fp);

				read_total += read_bytes;
				if (read_total <= pos1 || inject_completed2 || (inject_completed1 && read_total <= pos2)) {
					write_dst_file(dst_fp, buffer, read_bytes);
				} else if (!inject_completed1 && (read_total > pos1 && read_total > pos2)) {
					off_t offset1, offset2;

					offset1 = read_bytes - (read_total - pos1);
					offset2 = read_bytes - (read_total - pos2);

					/* inject hex value into position 1 */
					inject_single_hex(dst_fp, buffer, offset2, offset1, i);

					/* inject hex value into position 2 */
					inject_single_hex(dst_fp, buffer + offset2, read_bytes - offset2, 0, j);

					inject_completed2 = 1;
				} else {
					off_t offset, pos = (read_total > pos2) ? pos2 : pos1;

					/* calculate offset */
					offset = read_bytes - (read_total - pos);

					/* inject hex value into position */
					inject_single_hex(dst_fp, buffer, read_bytes, offset, (read_total > pos2) ? j : i);

					if (read_total > pos2)
						inject_completed2 = 1;
					else
						inject_completed1 = 1;
				}
			}

			/* seek file position into begin */
			fseek(fp, 0L, SEEK_SET);

			/* close destination file */
			fclose(dst_fp);
		}
	}

	/* close source file */
	fclose(fp);
}

/*
 * inject format string or character string
 */

static void str_inject_func_t34(const char *src_fpath, const char *dst_fpath, const char *pos_str,
	const char *count_str, int inject_format, int replace)
{
	FILE *fp, *dst_fp;
	off_t pos;

	off_t src_fsize;

	size_t read_total = 0;
	int inject_completed = 0;

	char *inject;
	int i, count;

	size_t total_inject_bytes = 0;

	/* parse position param */
	if (parse_position_param(pos_str, src_fpath, &pos) != 0)
		return;

	/* parse count */
	count = strtol(count_str, NULL, 10);
	if (count == LONG_MIN || count == LONG_MAX) {
		fprintf(stderr, "Invalid count value(%s)\n", count_str);
		return;
	}

	total_inject_bytes = inject_format ? count * 2 : count;

	src_fsize = get_fsize(src_fpath);
	if (replace && src_fsize < (pos + total_inject_bytes))
		total_inject_bytes = src_fsize - pos;

	/* set inject string */
	inject = (char *) malloc(total_inject_bytes + 1);
	if (!inject) {
		fprintf(stderr, "Out of memory!\n");
		return;
	}

	for (i = 0; i < total_inject_bytes; i++) {
		if (inject_format)
			inject[i] = i % 2 ? 's' : '%';
		else
			inject[i] = INJECT_STR_CHAR;
	}

	/* open source file */
	fp = open_file(src_fpath, 1);
	if (!fp) {
		fprintf(stderr, "Couln't open file '%s'\n", src_fpath);
		free(inject);
		return;
	}

	/* open destination file */
	dst_fp = open_file(dst_fpath, 0);
	if (!dst_fp) {
		fclose(fp);
		free(inject);
		return;
	}

	while (!feof(fp)) {
		size_t read_bytes;
		char buffer[MAX_BUF_LEN];

		/* read buffer from file */
		read_bytes = fread(buffer, 1, MAX_BUF_LEN, fp);

		read_total += read_bytes;
		if (read_total <= pos || inject_completed) {
			write_dst_file(dst_fp, buffer, read_bytes);
		} else {
			/* inject multiple hex values */
			inject_multiple_hex(fp, dst_fp, buffer, read_total, read_bytes,
				inject, total_inject_bytes, pos, replace);

			/* free inject buffer */
			free(inject);

			inject_completed = 1;
		}
	}

	/* close destination file */
	fclose(dst_fp);

	/* close source file */
	fclose(fp);
}

/*
 * inject each line from given file
 */

static void str_inject_func_t5(const char *src_fpath, const char *dst_fpath_templ, const char *pos_str,
	const char *inject_fpath, int replace)
{
	FILE *fp, *inject_fp;
	off_t pos;

	char inject_line[MAX_LINE_LEN + 1];
	int inject_fcount = 0;

	off_t src_fsize;

	/* parse position param */
	if (parse_position_param(pos_str, src_fpath, &pos) != 0)
		return;

	/* open source file */
	fp = open_file(src_fpath, 1);
	if (!fp) {
		fprintf(stderr, "Couln't open file '%s'\n", src_fpath);
		return;
	}

	src_fsize = get_fsize(src_fpath);

	/* open file which has inject lines */
	inject_fp = open_file(inject_fpath, 1);
	if (!inject_fpath) {
		fprintf(stderr, "Couln't open file '%s' which has inject lines\n", inject_fpath);

		fclose(fp);
		return;
	}

	while (fgets(inject_line, sizeof(inject_line), inject_fp) != NULL) {
		FILE *dst_fp;
		char dst_fpath[256];

		size_t read_total = 0;
		int inject_completed = 0;

		size_t inject_len;

		/* remove end line */
		if (strstr(inject_line, "/r/n"))
			inject_line[strlen(inject_line) - 2] = '\0';
		else if (inject_line[strlen(inject_line) - 1] == '\n')
			inject_line[strlen(inject_line) - 1] = '\0';

		/* check line length */
		inject_len = strlen(inject_line);
		if (inject_len == 0)
			continue;

		if (replace && src_fsize < (pos + inject_len))
			inject_len = src_fsize - pos;

		/* open destination file */
		snprintf(dst_fpath, sizeof(dst_fpath), "%s.%d", dst_fpath_templ, inject_fcount);
		dst_fp = open_file(dst_fpath, 0);
		if (!dst_fp) {
			fclose(fp);
			continue;
		}

		while (!feof(fp)) {
			size_t read_bytes;
			char buffer[MAX_BUF_LEN];

			/* read buffer from file */
			read_bytes = fread(buffer, 1, MAX_BUF_LEN, fp);

			read_total += read_bytes;
			if (read_total <= pos || inject_completed) {
				write_dst_file(dst_fp, buffer, read_bytes);
			} else {
				/* inject multiple hex values */
				inject_multiple_hex(fp, dst_fp, buffer, read_total, read_bytes, inject_line, inject_len, pos, replace);

				inject_completed = 1;
			}
		}

		/* close destination file */
		fclose(dst_fp);

		/* seek file position into begin */
		fseek(fp, 0L, SEEK_SET);

		/* increase inject file count */
		inject_fcount++;
	}

	/* close source file */
	fclose(fp);
}

/*
 * main function
 */

int main(int argc, char *argv[])
{
	enum STR_INJECT_TYPE inject_type = STR_INJECT_TYPE_UNKNOWN;
	int replace = 0;

	/* check argument */
	if (argc == 5 && strcmp(argv[3], "-h") == 0) {
		inject_type = strchr(argv[4], ',') ? STR_INJECT_TYPE_2 : STR_INJECT_TYPE_1;
	} else if (argc == 6 || argc == 7) {
		if (strcmp(argv[3], "-f") == 0)
			inject_type = STR_INJECT_TYPE_3;
		else if (strcmp(argv[3], "-o") == 0)
			inject_type = STR_INJECT_TYPE_4;
		else if (strcmp(argv[3], "-i") == 0)
			inject_type = STR_INJECT_TYPE_5;

		if (argc == 7 && strcmp(argv[6], "-r") == 0)
			replace = 1;
	} else {
		fprintf(stderr, "Invalid options.\n");
		print_help();
	}

	/* check inject type */
	if (inject_type == STR_INJECT_TYPE_UNKNOWN) {
		fprintf(stderr, "Unknown injection option '%s'\n", argv[3]);
		print_help();
	}

	switch (inject_type) {
	case STR_INJECT_TYPE_1:
		str_inject_func_t1(argv[1], argv[2], argv[4]);
		break;

	case STR_INJECT_TYPE_2:
		str_inject_func_t2(argv[1], argv[2], argv[4]);
		break;

	case STR_INJECT_TYPE_3:
		str_inject_func_t34(argv[1], argv[2], argv[4], argv[5], 1, replace);
		break;

	case STR_INJECT_TYPE_4:
		str_inject_func_t34(argv[1], argv[2], argv[4], argv[5], 0, replace);
		break;

	case STR_INJECT_TYPE_5:
		str_inject_func_t5(argv[1], argv[2], argv[4], argv[5], replace);
		break;

	default:
		break;
	}

	return 0;
}
