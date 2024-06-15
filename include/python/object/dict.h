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

#include "../object.h"

/*
 * Invariant for entries: when in use, value is not NULL and key is
 * not NULL and not dummy; when not in use, value is NULL and key
 * is either NULL or dummy. A dummy key value cannot be replaced by NULL,
 * since otherwise other keys may be lost.
 */
struct py_dictentry {
	struct py_object* key;
	struct py_object* value;
};

struct py_dict {
	struct py_object ob;

	unsigned fill;
	unsigned used;
	unsigned size;

	struct py_dictentry* table;
};

struct py_object* py_dict_new(void);

struct py_object* py_dict_lookup(struct py_object*, const char*);
struct py_object* py_dict_lookup_object(struct py_object*, struct py_object*);
int py_dict_assign(struct py_object*, struct py_object*, struct py_object*);
int py_dict_insert(struct py_object*, const char*, struct py_object*);
int py_dict_remove(struct py_object*, const char*);
unsigned py_dict_size(struct py_object*);
const char* py_dict_get_key(struct py_object*, unsigned);
void py_dict_dealloc(struct py_object*);

void py_done_dict(void);

#endif
