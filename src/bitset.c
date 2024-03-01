/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Bitset primitives used by the parser generator */

#include <python/errors.h>
#include <python/bitset.h>

py_bitset_t py_bitset_new(int nbits) {
	int nbytes = PY_NBYTES(nbits);
	py_bitset_t ss = malloc(nbytes * sizeof(py_byte_t));

	if(ss == NULL) py_fatal("no mem for bitset");

	ss += nbytes;
	while(--nbytes >= 0) *--ss = 0;

	return ss;
}

void py_bitset_delete(py_bitset_t ss) { free(ss); }

int py_bitset_add(py_bitset_t ss, int ibit) {
	int ibyte = ibit / CHAR_BIT;
	py_byte_t mask = PY_BIT2MASK(ibit);

	if(ss[ibyte] & mask) return 0; /* Bit already set */
	ss[ibyte] |= mask;

	return 1;
}

int py_bitset_cmp(py_bitset_t ss1, py_bitset_t ss2, int nbits) {
	int i;

	for(i = PY_NBYTES(nbits); --i >= 0;) {
		if(*ss1++ != *ss2++) return 0;
	}

	return 1;
}

void py_bitset_merge(py_bitset_t ss1, py_bitset_t ss2, int nbits) {
	int i;

	for(i = PY_NBYTES(nbits); --i >= 0;) *ss1++ |= *ss2++;
}
