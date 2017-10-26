
unsigned long long command_clock_monotonic (void);

__attribute__((unused)) static int command_clock_after (unsigned long long a, unsigned long long b)
{
	return (((long long) ((b) - (a)) < 0)) ? 1 : 0;
}
#define command_clock_before(a, b)        command_clock_after(b, a)
