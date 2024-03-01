/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Time module */

#ifdef _WIN32

# include <windows.h>

#endif

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/intobject.h>

/*
 * TODO: Remove all builtin modules from Python itself and move to AGA to make
 * 		 More sensible impls.
 */

/* TODO: Higher resolution timers. */

/* Time methods */

static struct py_object*
time_time(struct py_object* self, struct py_object* args) {
	long secs;

	(void) self;

	if(!py_arg_none(args)) {
		return NULL;
	}
	secs = time((time_t*) NULL);
	return py_int_new(secs);
}

static jmp_buf sleep_intr;

static void sleep_catcher(int sig) {
	(void) sig;

	longjmp(sleep_intr, 1);
}

static struct py_object*
time_sleep(struct py_object* self, struct py_object* args) {
	int secs;
	void (* sigsave)(int);

	(void) self;

	if(!py_arg_int(args, &secs)) {
		return NULL;
	}

	if(setjmp(sleep_intr)) {
		signal(SIGINT, sigsave);
		py_error_set(py_interrupt_error);
		return NULL;
	}

	sigsave = signal(SIGINT, SIG_IGN);
	if(sigsave != (void (*)(int)) SIG_IGN) {
		signal(SIGINT, sleep_catcher);
	}

#ifdef _WIN32
	Sleep(1000 * secs);
#else
	sleep(secs);
#endif

	signal(SIGINT, sigsave);
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_methodlist time_methods[] = {
		{ "sleep", time_sleep },
		{ "time",  time_time },
		{ NULL, NULL }           /* sentinel */
};


void inittime() {
	py_module_init("time", time_methods);
}


#ifdef THINK_C

#define MacTicks       (* (long *)0x16A)

static
sleep(msecs)
	   int msecs;
{
	   long deadline;

	   deadline = MacTicks + msecs * 60;
	   while (MacTicks < deadline) {
			   if (py_intrcheck())
					   sleep_catcher(SIGINT);
	   }
}

static
millisleep(msecs)
	   long msecs;
{
	   long deadline;

	   deadline = MacTicks + msecs * 3 / 50; /* msecs * 60 / 1000 */
	   while (MacTicks < deadline) {
			   if (py_intrcheck())
					   sleep_catcher(SIGINT);
	   }
}

static long
millitimer()
{
	   return MacTicks * 50 / 3; /* MacTicks * 1000 / 60 */
}

#endif /* THINK_C */


#ifdef BSD_TIME

#include <sys/types.h>
#include <sys/time.h>

static long
millitimer()
{
	   struct timeval t;
	   struct timezone tz;
	   if (gettimeofday(&t, &tz) != 0)
			   return -1;
	   return t.tv_sec*1000 + t.tv_usec/1000;

}

static
millisleep(msecs)
	   long msecs;
{
	   struct timeval t;
	   t.tv_sec = msecs/1000;
	   t.tv_usec = (msecs%1000)*1000;
	   (void) select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &t);
}

#endif /* BSD_TIME */

