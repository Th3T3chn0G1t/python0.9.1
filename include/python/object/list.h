/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* List object interface */

#ifndef PY_LISTOBJECT_H
#define PY_LISTOBJECT_H

#include <python/object.h>

/*
 * Another generally useful object type is an list of object pointers.
 * This is a mutable type: the list items can be changed, and items can be
 * added or removed. Out-of-range indices or non-list objects are ignored.
 *
 * *** WARNING *** py_list_set does not increment the new item's reference
 * count, but does decrement the reference count of the item it replaces,
 * if not nil. It does *decrement* the reference count if it is *not*
 * inserted in the list. Similarly, py_list_get does not increment the
 * returned item's reference count.
 */

struct py_list {
	struct py_varobject ob;
	struct py_object** item;
};

struct py_object* py_list_new(unsigned);
struct py_object* py_list_get(const struct py_object*, unsigned);
void py_list_set(struct py_object*, unsigned, struct py_object*);
int py_list_insert(struct py_object*, unsigned, struct py_object*);
int py_list_add(struct py_object*, struct py_object*);

void py_list_dealloc(struct py_object*);
int py_list_cmp(const struct py_object*, const struct py_object*);

struct py_object* py_list_cat(struct py_object*, struct py_object*);
struct py_object* py_list_ind(struct py_object*, unsigned);
struct py_object* py_list_slice(struct py_object*, unsigned, unsigned);

#endif
