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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

void print_usage(void)
{
	printf("Usage: writev_exmpl $FD $STR $SIZE $IOVCNT\n"
			"$FD: File descriptor number\n"
			"$STR: String to write\n"
			"$SIZE: Number of bytes of $STR to write\n"
			"$IOVCNT: IO vector size\n"
			"e.g. \"writev_exmpl 1 Hello 5 2\"\n");
}

int main(int argc, char *argv[])
{
	struct iovec *iovec;
	struct iovec *p;
	int iovcnt;
	int i;

	if (argc != 5) {
		print_usage();
		return -1;
	}

	iovcnt = atoi(argv[4]);
	p = iovec = (struct iovec *) malloc(sizeof(struct iovec) * iovcnt);

	for (i = 0; i != iovcnt; i++) {
		p->iov_base = argv[2];
		p->iov_len = atoi(argv[3]);
		p++;
	}

	return writev(atoi(argv[1]),
		iovec,
		iovcnt);
}
