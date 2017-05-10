
unsigned long mbus_clock_get (void);

static inline int mbus_clock_after (unsigned long a, unsigned long b)
{
	return (((long)((b) - (a)) < 0)) ? 1 : 0;
}
#define mbus_clock_before(a,b)        clock_after(b,a)
