
enum command_waitpid_option {
	command_waitpid_option_none	= 0x00000000,
	command_waitpid_option_nohang	= 0x00000001,
};

pid_t command_exec (char * const *args, const char **environment, int *io);
int command_waitpid (pid_t pid, int *status, enum command_waitpid_option option);
int command_kill (pid_t pid, int sig);
