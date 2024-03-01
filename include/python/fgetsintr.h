/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_FGETSINTR_H
#define PY_FGETSINTR_H

#include <python/std.h>

/* TODO: Deal with/remove Python's interrupt handling. */

int py_fgets_intr(char*, int, FILE*);

int py_intrcheck(void);
void py_initintr(void);

#endif
