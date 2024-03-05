/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Stdlib includes */

#ifndef PY_STD_H
#define PY_STD_H

#ifdef _WIN32
# ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
# endif
#endif

#ifndef _MSC_VER
# ifndef _SVID_SOURCE
/*
 * NOTE: This is to circumvent the glibc warning, we don't really want
 * 		 Everything this claims to expose.
 */
#  define _DEFAULT_SOURCE
#  define _SVID_SOURCE
# endif
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4710)
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

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#ifdef _WIN32
# undef _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _MSC_VER
# undef _DEFAULT_SOURCE
# undef _SVID_SOURCE
#endif

#endif
