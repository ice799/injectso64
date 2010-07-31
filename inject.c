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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stddef.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>
//#include <linux/user.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <dlfcn.h>

/* from linux/user.h which disappeared recently: */
struct my_user_regs {
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long rbp;
	unsigned long rbx;
/* arguments: non interrupts/non tracing syscalls only save upto here*/
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long rax;
	unsigned long rcx;
	unsigned long rdx;
	unsigned long rsi;
	unsigned long rdi;
	unsigned long orig_rax;
/* end of arguments */
/* cpu exception frame or undefined */
	unsigned long rip;
	unsigned long cs;
	unsigned long eflags;
	unsigned long rsp;
	unsigned long ss;
	unsigned long fs_base;
	unsigned long gs_base;
	unsigned long ds;
	unsigned long es;
	unsigned long fs;
	unsigned long gs;
};


/* do_dlopen (not used anymore) */
struct do_dlopen_args
{
	/* Argument to do_dlopen.  */
	char *name;
	/* Opening mode.  */
	int mode;

	/* Return from do_dlopen.  */
	void *map;
};


/* The glibc function we are going to call
 * __libc_dlopen_mode()
 */

void die(const char *s)
{
	perror(s);
	exit(errno);
}


char *find_libc_start(pid_t pid)
{
	char path[1024];
	char buf[1024], *start = NULL, *end = NULL, *p = NULL;
	char *addr1 = NULL, *addr2 = NULL;
	FILE *f = NULL;
	

	snprintf(path, sizeof(path), "/proc/%d/maps", pid);

	if ((f = fopen(path, "r")) == NULL)
		die("fopen");

	for (;;) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if (!strstr(buf, "r-xp"))
			continue;
		if (!(p = strstr(buf, "/")))
			continue;
		if (!strstr(p, "/lib64") || !strstr(p, "/libc-"))
			continue;
		start = strtok(buf, "-");
		addr1 = (char *)strtoul(start, NULL, 16);
		end = strtok(NULL, " ");
		addr2 = (char *)strtoul(end, NULL, 16);
		break;
	}

	fclose(f);	
	return addr1;
}


int poke_text(pid_t pid, size_t addr, char *buf, size_t blen)
{
	int i = 0;
	char *ptr = malloc(blen + blen % sizeof(size_t));	// word align
	memcpy(ptr, buf, blen);

	for (i = 0; i < blen; i += sizeof(size_t)) {
		if (ptrace(PTRACE_POKETEXT, pid, addr + i, *(size_t *)&ptr[i]) < 0)
			die("ptrace");
	}
	free(ptr);
	return 0;
}



int peek_text(pid_t pid, size_t addr, char *buf, size_t blen)
{
	int i = 0;
	size_t word = 0;
	for (i = 0; i < blen; i += sizeof(size_t)) {
		word = ptrace(PTRACE_PEEKTEXT,pid, addr + i, NULL);
		memcpy(&buf[i], &word, sizeof(word));
	}
	return 0;
}


int inject_code(pid_t pid, size_t libc_addr, size_t dlopen_addr, char *dso)
{
	char sbuf1[1024], sbuf2[1024];
	struct my_user_regs regs, saved_regs;
	int status;

	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0)
		die("ptrace 1");
	waitpid(pid, &status, 0);
	if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
		die("ptrace 2");

	peek_text(pid, regs.rsp + 1024, sbuf1, sizeof(sbuf1));
	peek_text(pid, regs.rsp, sbuf2, sizeof(sbuf2));

	/* fake saved return address */
	libc_addr = 0x0;
	poke_text(pid, regs.rsp, (char *)&libc_addr, sizeof(libc_addr));
	poke_text(pid, regs.rsp + 1024, dso, strlen(dso) + 1); 

	memcpy(&saved_regs, &regs, sizeof(regs));

	/* pointer to &args */
	printf("rdi=%zx rsp=%zx rip=%zx\n", regs.rdi, regs.rsp, regs.rip);

	regs.rdi = regs.rsp + 1024;
	regs.rsi = RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE;
	regs.rip = dlopen_addr + 2;// kernel bug?! always need to add 2!

	if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0)
		die("ptrace 3");
	if (ptrace(PTRACE_CONT, pid, NULL, NULL) < 0)
		die("ptrace 4");

	/* Should receive a SIGSEGV */
	waitpid(pid, &status, 0);

	if (ptrace(PTRACE_SETREGS, pid, 0, &saved_regs) < 0)
		die("ptrace 5");

	poke_text(pid, saved_regs.rsp + 1024, sbuf1, sizeof(sbuf1));
	poke_text(pid, saved_regs.rsp, sbuf2, sizeof(sbuf2));

	if (ptrace(PTRACE_DETACH, pid, NULL, NULL) < 0)
		die("ptrace 6");

	return 0;
}



void usage(const char *path)
{
	printf("Usage: %s <pid> <dso-path>\n", path);
}


int main(int argc, char **argv)
{
	pid_t daemon_pid = -1;
	char *my_libc = NULL, *daemon_libc = NULL;
	char *dl_open_address = NULL;
	char *dlopen_mode = NULL;
	FILE *pfd = NULL;
	char buf[128], *space = NULL;

	/* nm /lib64/libc.so.6|grep __libc_dlopen_mode: 00000000000f2a40 t __libc_dlopen_mode */
	size_t dlopen_offset = 0;

	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	setbuffer(stdout, NULL, 0);

	my_libc = find_libc_start(getpid());
	
	printf("Trying to obtain __libc_dlopen_mode() address relative to libc start address.\n");
	printf("[1] Using my own __libc_dlopen_mode ...\n");
	dlopen_mode = dlsym(NULL, "__libc_dlopen_mode");
	if (dlopen_mode)
		dlopen_offset = dlopen_mode - my_libc;
		
	if (dlopen_offset == 0 && 
	    (pfd = popen("nm /lib64/libc.so.6|grep __libc_dlopen_mode", "r")) != NULL) {
		printf("[2] Using nm method ... ");
		fgets(buf, sizeof(buf), pfd);
		if ((space = strchr(buf, ' ')) != NULL)
			*space = 0;
		dlopen_offset = strtoul(buf, NULL, 16);
		fclose(pfd);
	}
	if (dlopen_offset == 0) {
		printf("failed!\nNo more methods, bailing out.\n");
		return 1;
	}
	printf("success!\n");

	dl_open_address = find_libc_start(getpid()) + dlopen_offset;
	daemon_pid = (pid_t)atoi(argv[1]);
	daemon_libc = find_libc_start(daemon_pid);

	printf("me: {__libc_dlopen_mode:%p, dlopen_offset:%zx}\n=> daemon: {__libc_dlopen_mode:%p, libc:%p}\n",
	       dl_open_address, dlopen_offset, daemon_libc + dlopen_offset, daemon_libc);

	inject_code(daemon_pid, (size_t)daemon_libc,
	            (size_t)(daemon_libc + dlopen_offset), argv[2]);

	printf("done!\n");
	return 0;
}

