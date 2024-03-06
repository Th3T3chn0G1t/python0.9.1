/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Bitset interface */

#ifndef PY_BITSET_H
#define PY_BITSET_H

#include <python/std.h>

#define PY_NBYTES(nbits) ((unsigned) (((nbits) + CHAR_BIT - 1) / CHAR_BIT))
#define PY_BIT2MASK(ibit) (1 << ((ibit) % CHAR_BIT))
#define PY_TESTBIT(ss, ibit) \
    (((ss)[(ibit) / CHAR_BIT] & PY_BIT2MASK(ibit)) != 0)

typedef unsigned char py_byte_t;
typedef py_byte_t* py_bitset_t;

py_bitset_t py_bitset_new(int);

void py_bitset_delete(py_bitset_t);

int py_bitset_add(py_bitset_t, int); /* Returns 0 if already set */
int py_bitset_cmp(py_bitset_t, py_bitset_t, int);

void py_bitset_merge(py_bitset_t, py_bitset_t, int);

#endif
