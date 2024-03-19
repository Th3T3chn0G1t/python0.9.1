/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Frame object interface */

#ifndef PY_FRAMEOBJECT_H
#define PY_FRAMEOBJECT_H

#include <python/object.h>
#include <python/opcode.h>

struct py_block {
	enum py_opcode type; /* what kind of block this is */
	unsigned handler; /* where to jump to find handler */
	unsigned level; /* value stack level to pop to */
};

struct py_frame {
	struct py_object ob;

	/* TODO: Do all of these need to be objects? */
	struct py_frame* back; /* previous frame, or NULL */
	struct py_code* code; /* code segment */
	struct py_object* globals; /* global symbol table (struct py_dict) */
	struct py_object* locals; /* local symbol table (struct py_dict) */
	struct py_object** valuestack; /* malloc'ed array */
	struct py_block* blockstack; /* malloc'ed array */
	unsigned nvalues; /* size of valuestack */
	unsigned nblocks; /* size of blockstack */
	unsigned iblock; /* index in blockstack */
};

/* Standard object interface */

struct py_frame* py_frame_new(
		struct py_frame*, struct py_code*, struct py_object*,
		struct py_object*, unsigned, unsigned);
void py_frame_dealloc(struct py_object*);

/* The rest of the interface is specific for frame objects */

/* Block management functions */
void py_block_setup(struct py_frame*, enum py_opcode, unsigned, unsigned);
struct py_block* py_block_pop(struct py_frame*);

#endif
