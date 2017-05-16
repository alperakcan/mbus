
#if defined(__APPLE__) && defined(__MACH__)

#include <time.h>
#include <errno.h>
#include <sys/sysctl.h>

unsigned long mbus_clock_get (void)
{
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) < 0) {
        return 0;
    }
    time_t bsec = boottime.tv_sec, csec = time(NULL);

    return ((unsigned long) difftime(csec, bsec)) * 1000;
}

#else

#include <time.h>
#include <sys/sysinfo.h>

unsigned long mbus_clock_get (void)
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime * 1000;
}

#endif
