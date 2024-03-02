/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* String object interface */

#ifndef PY_STRINGOBJECT_H
#define PY_STRINGOBJECT_H

#include <python/object.h>

/*
 * Type struct py_string represents a character string. An extra zero byte is
 * reserved at the end to ensure it is zero-terminated, but a size is
 * present so strings with null bytes in them can be represented. This
 * is an immutable object type.
 *
 * There are functions to create new string objects, to test
 * an object for string-ness, and to get the
 * string value. The latter function returns a null pointer
 * if the object is not of the proper type.
 * There is a variant that takes an explicit size as well as a
 * variant that assumes a zero-terminated string. Note that none of the
 * functions should be applied to nil objects.
 */

/* NB The type is revealed here only because it is used in dictobject.c */

struct py_string {
	PY_VAROB_SEQ
	char value[1];
};

/* TODO: Python global state. */
extern struct py_type py_string_type;

#define py_is_string(op) ((op)->type == &py_string_type)

struct py_object* py_string_new_size(const char*, int);
struct py_object* py_string_new(const char*);
unsigned int py_string_size(struct py_object*);
char* py_string_get_value(struct py_object*);
void py_string_join(struct py_object**, struct py_object*);
int py_string_resize(struct py_object**, int);

/* Macro, trading safety for speed */
#define GETSTRINGVALUE(op) ((op)->value)
#define GETUSTRINGVALUE(s) ((unsigned char*) GETSTRINGVALUE(s))

#endif
