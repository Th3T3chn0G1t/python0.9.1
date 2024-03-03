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

/*
 * Invariant for entries: when in use, de_value is not NULL and de_key is
 * not NULL and not dummy; when not in use, de_value is NULL and de_key
 * is either NULL or dummy. A dummy key value cannot be replaced by NULL,
 * since otherwise other keys may be lost.
 */
struct py_dictentry {
	struct py_string* de_key;
	struct py_object* de_value;
};

struct py_dict {
	struct py_object ob;
	unsigned di_fill;
	unsigned di_used;
	unsigned di_size;
	struct py_dictentry* di_table;
};

/* TODO: Python global state. */
extern struct py_type py_dict_type;

#define py_is_dict(op) ((op)->type == &py_dict_type)

struct py_object* py_dict_new(void);

struct py_object* py_dict_lookup(struct py_object*, const char*);
struct py_object* py_dict_lookup_object(struct py_object*, struct py_object*);
int py_dict_assign(struct py_object*, struct py_object*, struct py_object*);
int py_dict_insert(struct py_object*, const char*, struct py_object*);
int py_dict_remove(struct py_object*, const char*);
unsigned py_dict_size(struct py_object*);
char* py_dict_get_key(struct py_object*, unsigned);
struct py_object* py_dict_get_keys(struct py_object*);

void py_done_dict(void);

#endif
