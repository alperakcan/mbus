
/*
 * Copyright (c) 2014, Alper Akcan <alper.akcan@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <Alper Akcan> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MBUS_DEBUG_NAME	"mbus-exec"

#include "mbus/debug.h"
#include "exec.h"

pid_t mbus_exec (char * const *args, int *io)
{
	int i;
	int n;
	int p[2];
	pid_t pid;
	pid_t gpid;

	n = -1;
	p[0] = -1;
	p[1] = -1;

	if (io != NULL) {
		if (pipe(p) < 0) {
			return -1;
		}
	} else {
		n = open("/dev/null", O_RDWR);
		if (n < 0) {
			return -1;
		}
	}

	gpid = getpid();
	mbus_debugf("gpid: %d", gpid);

	if ((pid = fork()) > 0) {
		if (io != NULL) {
			io[0] = p[0];
			io[1] = p[1];
		}
		return pid;
	} else if (pid == 0) {
		fflush(stdin);
		fflush(stdout);
		fflush(stderr);

#if 0
		setpgid(0, gpid);
		prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

		if (io == NULL) {
#if 0
			close(0);
			if (dup(n) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(1);
			if (dup(n) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(2);
			if (dup(n) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(n);
#endif
		} else {
			close(0);
			if (dup(p[0]) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(1);
			if (dup(p[1]) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(2);
			if (dup(p[1]) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(p[0]);
			close(p[1]);
		}

		for (i = 3; i < 1024; i++) {
			close(i);
		}
		execvp(args[0], args);
		mbus_errorf("execl() failed");
		exit(-1);
	} else {
		if (io != NULL) {
			close(p[0]);
			close(p[1]);
		} else {
			close(n);
		}
		mbus_errorf("fork() failure");
	}

	return -1;
}

int mbus_waitpid (pid_t pid, int *status, enum mbus_waitpid_option option)
{
	int o;
	o = 0;
	switch (option) {
		case mbus_waitpid_option_nohang:
			o |= WNOHANG;
			break;
	}
	return waitpid(pid, status, o);
}

int mbus_system (const char *command)
{
	if (command == NULL) {
		return -1;
	}
	return system(command);
}

int mbus_kill (pid_t pid)
{
	return kill(pid, SIGKILL);
}
