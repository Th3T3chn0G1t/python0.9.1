/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/*
 * Dictionary object type -- mapping from char * to object.
 * NB: the key is given as a char *, not as a struct py_string.
 * These functions set errno for errors. Functions py_dict_remove() and
 * py_dict_insert() return nonzero for errors, py_dict_size() returns -1,
 * the others NULL. A successful call to py_dict_insert() calls py_object_incref()
 * for the inserted item.
 */

#ifndef PY_DICTOBJECT_H
#define PY_DICTOBJECT_H

#include <python/object.h>

/* TODO: Python global state. */
extern struct py_type py_dict_type;

#define py_is_dict(op) ((op)->type == &py_dict_type)

struct py_object* py_dict_new(void);

struct py_object* py_dict_lookup(struct py_object* dp, const char* key);
int py_dict_insert(struct py_object* dp, const char* key, struct py_object* item);
int py_dict_remove(struct py_object* dp, const char* key);
int py_dict_size(struct py_object* dp);
char* py_dict_get_key(struct py_object* dp, int i);
struct py_object* py_dict_get_keys(struct py_object* dp);

void py_done_dict(void);

#endif
