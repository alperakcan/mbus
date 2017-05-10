
#include <time.h>
#include <sys/sysinfo.h>

unsigned long mbus_clock_get (void)
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime * 1000;
}
