
#include <time.h>

unsigned long mbus_clock_get (void)
{
	struct timespec ts;
	unsigned long long tsec;
	unsigned long long tusec;
	unsigned long long _clock;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) < 0) {
		return 0;
	}
	tsec = ((unsigned long long) ts.tv_sec) * 1000;
	tusec = ((unsigned long long) ts.tv_nsec) / 1000 / 1000;
	_clock = tsec + tusec;
	return _clock;
}
