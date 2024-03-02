/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Definitions for compiled intermediate code */

#ifndef PY_COMPILE_H
#define PY_COMPILE_H

#include <python/object.h>
#include <python/stringobject.h>
#include <python/node.h>

/* An intermediate code fragment contains:
   - a string that encodes the instructions,
   - a list of the constants,
   - and a list of the names used. */

struct py_code {
	struct py_object ob;

	struct py_string* code;  /* instruction opcodes */
	struct py_object* consts;      /* list of immutable constant objects */
	struct py_object* names;       /* list of stringobjects */
	struct py_object* filename;    /* string */
};

/* TODO: Python global state. */
extern struct py_type py_code_type;

#define py_is_code(op) (((struct py_object*) (op))->type == &py_code_type)

struct py_code* py_compile(struct py_node*, char *);

#endif
