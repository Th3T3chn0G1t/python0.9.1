/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Interface to map C struct members to Python object attributes */

#ifndef PY_STRUCTMEMBER_H
#define PY_STRUCTMEMBER_H

#include <python/std.h>

/*
 * An array of memberlist structures defines the name, type and offset
 * of selected members of a C structure. These can be read by
 * py_memberlist_get() and set by py_memberlist_set() (except if their PY_READONLY flag
 * is set). The array must be terminated with an entry whose name
 * pointer is NULL.
 */

/* Types */
enum py_structtype {
	PY_TYPE_SHORT = 0,
	PY_TYPE_INT = 1,
	PY_TYPE_LONG = 2,
	PY_TYPE_FLOAT = 3,
	PY_TYPE_DOUBLE = 4,
	PY_TYPE_STRING = 5,
	PY_TYPE_OBJECT = 6
};

enum py_readonly {
	PY_READWRITE,
	PY_READONLY
};

struct py_memberlist {
	char* name;
	enum py_structtype type;
	int offset;
	enum py_readonly readonly;
};

struct py_object* py_memberlist_get(char*, struct py_memberlist*, const char*);
int py_memberlist_set(char*, struct py_memberlist*, char*, struct py_object*);

#endif
