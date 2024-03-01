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

/* Bitset primitives used by the parser generator */

#include <stdlib.h>

#include <python/pgenheaders.h>
#include <python/bitset.h>

py_bitset_t newbitset(int nbits) {
	int nbytes = PY_NBYTES(nbits);
	py_bitset_t ss = malloc(nbytes * sizeof(py_byte_t));

	if(ss == NULL) fatal("no mem for bitset");

	ss += nbytes;
	while(--nbytes >= 0) *--ss = 0;

	return ss;
}

void delbitset(py_bitset_t ss) { free(ss); }

int addbit(py_bitset_t ss, int ibit) {
	int ibyte = PY_BIT2BYTE(ibit);
	py_byte_t mask = PY_BIT2MASK(ibit);

	if(ss[ibyte] & mask) return 0; /* Bit already set */
	ss[ibyte] |= mask;

	return 1;
}

int samebitset(py_bitset_t ss1, py_bitset_t ss2, int nbits) {
	int i;

	for(i = PY_NBYTES(nbits); --i >= 0;) {
		if(*ss1++ != *ss2++) return 0;
	}

	return 1;
}

void mergebitset(py_bitset_t ss1, py_bitset_t ss2, int nbits) {
	int i;

	for(i = PY_NBYTES(nbits); --i >= 0;) *ss1++ |= *ss2++;
}
