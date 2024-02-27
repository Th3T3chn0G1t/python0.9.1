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

/* Bitset interface */

#ifndef PY_BITSET_H
#define PY_BITSET_H

typedef char py_byte_t;
typedef py_byte_t* py_bitset_t;

py_bitset_t newbitset(int nbits);
void delbitset(py_bitset_t bs);
int addbit(py_bitset_t bs, int ibit); /* Returns 0 if already set */
int samebitset(py_bitset_t bs1, py_bitset_t bs2, int nbits);
void mergebitset(py_bitset_t bs1, py_bitset_t bs2, int nbits);

#define PY_BITSPERBYTE (8 * sizeof(py_byte_t))
#define PY_NBYTES(nbits) (((nbits) + PY_BITSPERBYTE - 1) / PY_BITSPERBYTE)

#define PY_BIT2BYTE(ibit) ((ibit) / PY_BITSPERBYTE)
#define PY_BIT2SHIFT(ibit) ((ibit) % PY_BITSPERBYTE)
#define PY_BIT2MASK(ibit) (1 << PY_BIT2SHIFT(ibit))
#define PY_BYTE2BIT(ibyte) ((ibyte) * PY_BITSPERBYTE)
#define PY_TESTBIT(ss, ibit) \
	(((ss)[PY_BIT2BYTE(ibit)] & PY_BIT2MASK(ibit)) != 0)

#endif
