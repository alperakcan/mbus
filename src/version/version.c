
#if !defined(GIT_COMMIT)
#define GIT_COMMIT "unknown"
#endif

#if !defined(GIT_REVISION)
#define GIT_REVISION "unknown"
#endif

const char * mbus_git_commit (void)
{
	return GIT_COMMIT;
}

const char * mbus_git_revision (void)
{
	return GIT_REVISION;
}
