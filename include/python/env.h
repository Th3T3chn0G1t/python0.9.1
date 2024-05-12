/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_ENV_H
#define PY_ENV_H

#include <python/std.h>

FILE* py_open_r(const char* path);

void py_close(FILE* fp);

#ifdef __has_attribute
# if __has_attribute(fallthrough)
#  define PY_FALLTHROUGH __attribute__((fallthrough))
# endif
#endif

#ifndef PY_FALLTHROUGH
# define PY_FALLTHROUGH
#endif

#endif
