
#define _GNU_SOURCE

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

#include <sys/prctl.h>

#define MBUS_DEBUG_NAME	"command-exec"

#include "mbus/debug.h"
#include "exec.h"

pid_t command_exec (char * const *args, const char **environment, int *io)
{
	int i;
	int j;
	int n;
	int in[2];
	int out[2];
	int err[2];
	const char **env;
	pid_t pid;

	n = -1;
	in[0] = -1;
	in[1] = -1;
	out[0] = -1;
	out[1] = -1;
	err[0] = -1;
	err[1] = -1;

	env = NULL;

	if (environment != NULL) {
		for (i = 0; environ[i] != NULL; i++);
		n = i;
		for (i = 0; environment[i] != NULL; i++);
		n += i;
		env = malloc((n + 1) * sizeof(*env));
		if (env == NULL) {
			mbus_errorf("malloc(env) failed for command: %s", args[0]);
			goto bail;
		}
		n = 0;
		for (i = 0; environ[i] != NULL; i++) {
			env[n++] = environ[i];
		}
		for (i = 0; environment[i] != NULL; i++) {
			for (j = 0; j < n; j++) {
				if (strncmp(env[j], environment[i], strcspn(environment[i], "=") + 1) == 0) {
					env[j] = environment[i];
					break;
				}
			}
			if (j >= n) {
				env[n++] = environment[i];
			}
		}
		env[n++] = NULL;
	}

	if (io != NULL) {
		if (pipe(in) < 0) {
			goto bail;
		}
		if (pipe(out) < 0) {
			goto bail;
		}
		if (pipe(err) < 0) {
			goto bail;
		}
	} else {
		n = open("/dev/null", O_RDWR);
		if (n < 0) {
			goto bail;
		}
	}

	if ((pid = fork()) > 0) {
		if (io != NULL) {
			io[0] = in[1];
			io[1] = out[0];
			io[2] = err[0];
			close(in[0]);
			close(out[1]);
			close(err[1]);
		}
		if (env != NULL) {
			free(env);
		}
		return pid;
	} else if (pid == 0) {
		setpgid(0, 0);
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		fflush(stdin);
		fflush(stdout);
		fflush(stderr);
		if (io == NULL) {
#if 0
			if (dup2(n, STDIN_FILENO) < 0) {
				perror("dup of write side of pipe failed");
			}
			if (dup2(n, STDOUT_FILENO) < 0) {
				perror("dup of write side of pipe failed");
			}
			if (dup2(n, STDERR_FILENO) < 0) {
				perror("dup of write side of pipe failed");
			}
			close(n);
#endif
		} else {
#if 1
			dup2(in[0], STDIN_FILENO);
			dup2(out[1], STDOUT_FILENO);
			dup2(err[1], STDERR_FILENO);
			close(in[0]);
			close(in[1]);
			close(out[0]);
			close(out[1]);
			close(err[0]);
			close(err[1]);
#endif
		}
		for (i = 3; i < 1024 && 0; i++) {
			close(i);
		}
		execvpe(args[0], args, (env != NULL) ? ((char * const *) env) : (environ));
		mbus_errorf("execl(%s) failed", args[0]);
		if (env != NULL) {
			free(env);
		}
		exit(-1);
	}

	mbus_errorf("fork() failure");

bail:	if (io != NULL) {
		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
	} else {
		close(n);
	}
	if (env != NULL) {
		free(env);
	}
	return -1;
}

int command_waitpid (pid_t pid, int *status, enum command_waitpid_option option)
{
	int o;
	o = 0;
	switch (option) {
		case command_waitpid_option_none:
			break;
		case command_waitpid_option_nohang:
			o |= WNOHANG;
			break;
	}
	return waitpid(pid, status, o);
}

int command_kill (pid_t pid, int sig)
{
	return kill((pid < 0) ? pid : -pid, sig);
}
