/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Interruptable version of fgets().
   Return < 0 for interrupted, 1 for EOF, 0 for valid input. */

/* XXX This uses longjmp() from a signal out of fgets().
   Should use read() instead?! */

#include <signal.h>
#include <setjmp.h>

#include <python/result.h>
#include <python/fgetsintr.h>

static jmp_buf jback;

static void catcher(int sig) {
	(void) sig;

	longjmp(jback, 1);
}

int py_fgets_intr(buf, size, fp)char* buf;
								int size;
								FILE* fp;
{
	int ret;
	void (* sigsave)();

	if(setjmp(jback)) {
		clearerr(fp);
		signal(SIGINT, sigsave);

		return PY_RESULT_INTERRUPT;
	}

	/* The casts to (void(*)()) are needed by THINK_C only */

	sigsave = signal(SIGINT, (void (*)()) SIG_IGN);
	if(sigsave != (void (*)()) SIG_IGN) {
		signal(SIGINT, (void (*)()) catcher);
	}

	if(py_intrcheck()) { ret = PY_RESULT_INTERRUPT; }
	else {
		ret = (fgets(buf, size, fp) == NULL) ? PY_RESULT_EOF : PY_RESULT_OK;
	}

	if(sigsave != (void (*)()) SIG_IGN) {
		signal(SIGINT, (void (*)()) sigsave);
	}
	return ret;
}
