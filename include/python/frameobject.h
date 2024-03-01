/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Frame object interface */

#ifndef PY_FRAMEOBJECT_H
#define PY_FRAMEOBJECT_H

#include <python/object.h>

struct py_block {
	int type; /* what kind of block this is */
	int handler; /* where to jump to find handler */
	int level; /* value stack level to pop to */
};

struct py_frame {
	PY_OB_SEQ
	struct py_frame* back; /* previous frame, or NULL */
	struct py_code* code; /* code segment */
	struct py_object* globals; /* global symbol table (dictobject) */
	struct py_object* locals; /* local symbol table (dictobject) */
	struct py_object** valuestack; /* malloc'ed array */
	struct py_block* blockstack; /* malloc'ed array */
	int nvalues; /* size of valuestack */
	int nblocks; /* size of blockstack */
	int iblock; /* index in blockstack */
};

/* Standard object interface */

/* TODO: Python global state. */
extern struct py_type py_frame_type;

#define py_is_frame(op) ((op)->type == &py_frame_type)

struct py_frame* py_frame_new(struct py_frame*, struct py_code*, struct py_object*, struct py_object*, int, int);

/* The rest of the interface is specific for frame objects */

/* Block management functions */
void py_block_setup(struct py_frame*, int, int, int);
struct py_block* py_block_pop(struct py_frame*);

#endif
