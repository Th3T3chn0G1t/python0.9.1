/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Bitset primitives used by the parser generator */

#include <python/bitset.h>

py_bitset_t py_bitset_new(unsigned nbits) {
	return calloc(PY_NBYTES(nbits), sizeof(py_byte_t));
}

void py_bitset_delete(py_bitset_t ss) {
	free(ss);
}

int py_bitset_add(py_bitset_t ss, unsigned ibit) {
	unsigned ibyte = ibit / CHAR_BIT;
	py_byte_t mask = PY_BIT2MASK(ibit);

	if(ss[ibyte] & mask) return 0; /* Bit already set */
	ss[ibyte] |= mask;

	return 1;
}

int py_bitset_cmp(py_bitset_t ss1, py_bitset_t ss2, unsigned nbits) {
	unsigned i;

	for(i = 0; i < PY_NBYTES(nbits); ++i) {
		if(*ss1++ != *ss2++) return 0;
	}

	return 1;
}

void py_bitset_merge(py_bitset_t ss1, py_bitset_t ss2, unsigned nbits) {
	unsigned i;

	for(i = 0; i < PY_NBYTES(nbits); ++i) *ss1++ |= *ss2++;
}
