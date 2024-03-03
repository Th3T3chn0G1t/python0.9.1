/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Stdlib includes */

#ifndef PY_STD_H
#define PY_STD_H

#ifndef _SVID_SOURCE
/*
 * NOTE: This is to circumvent the glibc warning, we don't really want
 * 		 Everything this claims to expose.
 */
# define _DEFAULT_SOURCE
# define _SVID_SOURCE
#endif

#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#endif
