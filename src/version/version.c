
#if !defined(GIT_COMMIT)
#define GIT_COMMIT "unknown"
#endif

#if !defined(GIT_VERSION)
#define GIT_VERSION "unknown"
#endif

const char * mbus_git_commit (void)
{
	return GIT_COMMIT;
}

const char * mbus_git_version (void)
{
	return GIT_VERSION;
}
