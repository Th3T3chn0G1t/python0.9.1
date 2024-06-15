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
	struct py_varobject ob;
	char value[1]; /* TODO: Is this supposed to be sized? FAM? */
};

struct py_object* py_string_new_size(const char*, unsigned);
struct py_object* py_string_new(const char*);
const char* py_string_get(const struct py_object*);

struct py_object* py_string_cat(struct py_object*, struct py_object*);
struct py_object* py_string_ind(struct py_object*, unsigned);
struct py_object* py_string_slice(struct py_object*, unsigned, unsigned);

int py_string_cmp(const struct py_object*, const struct py_object*);

#endif
