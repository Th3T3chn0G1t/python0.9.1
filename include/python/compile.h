/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Definitions for compiled intermediate code */

#ifndef PY_COMPILE_H
#define PY_COMPILE_H

#include <python/node.h>
#include <python/bitset.h>

#include <python/object.h>

/*
 * An intermediate code fragment contains:
 * - a string that encodes the instructions,
 * - a list of the constants,
 * - and a list of the names used.
 */

struct py_code {
	struct py_object ob;

	py_byte_t* code; /* instruction opcodes */
	/* TODO: Do these need to be objects? */
	struct py_object* consts; /* list of immutable constant objects */
	struct py_object* names; /* list of stringobjects */
	struct py_object* filename; /* string */
};

struct py_code* py_compile(struct py_node*, const char*);
void py_code_dealloc(struct py_object*);

#endif
