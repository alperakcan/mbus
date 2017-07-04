
#include <stdio.h>
#include "mbus/version.h"

int main (int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	fprintf(stdout, "mbus version\n");
	fprintf(stdout, "  git-commit  : %s\n", mbus_git_commit());
	fprintf(stdout, "  git-revision: %s\n", mbus_git_revision());
	return 0;
}
