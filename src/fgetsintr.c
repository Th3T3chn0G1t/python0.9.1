/***********************************************************
Copyright 1991 by Stichting Mathematisch Centrum, Amsterdam, The
Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/* Interruptable version of fgets().
   Return < 0 for interrupted, 1 for EOF, 0 for valid input. */

/* XXX This uses longjmp() from a signal out of fgets().
   Should use read() instead?! */

#include "pgenheaders.h"

#include <signal.h>
#include <setjmp.h>

#include "errcode.h"
#include "fgetsintr.h"

static jmp_buf jback;

static void catcher(int sig) {
	(void) sig;

	longjmp(jback, 1);
}

int fgets_intr(buf, size, fp)char* buf;
							 int size;
							 FILE* fp;
{
	int ret;
	void (* sigsave)();

	if(setjmp(jback)) {
		clearerr(fp);
		signal(SIGINT, sigsave);

		return E_INTR;
	}

	/* The casts to (void(*)()) are needed by THINK_C only */

	sigsave = signal(SIGINT, (void (*)()) SIG_IGN);
	if(sigsave != (void (*)()) SIG_IGN) {
		signal(SIGINT, (void (*)()) catcher);
	}

	if(intrcheck()) ret = E_INTR;
	else ret = (fgets(buf, size, fp) == NULL) ? E_EOF : E_OK;

	if(sigsave != (void (*)()) SIG_IGN) {
		signal(SIGINT, (void (*)()) sigsave);
	}
	return ret;
}
