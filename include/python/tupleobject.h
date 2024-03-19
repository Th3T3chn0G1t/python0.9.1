/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Tuple object interface */

#ifndef PY_TUPLEOBJECT_H
#define PY_TUPLEOBJECT_H

/*
 * Another generally useful object type is a tuple of object pointers.
 * This is a mutable type: the tuple items can be changed (but not their
 * number). Out-of-range indices or non-tuple objects are ignored.
 */

/*
 * NOTE: `py_tuple_set' does not increment the new item's reference
 * count, but does decrement the reference count of the item it replaces,
 * if not nil. It does *decrement* the reference count if it is *not*
 * inserted in the tuple. Similarly, py_tuple_get does not increment the
 * returned item's reference count.
 */

struct py_tuple {
	struct py_varobject ob;
	struct py_object* item[1]; /* TODO: FAM? */
};

struct py_object* py_tuple_new(unsigned);
struct py_object* py_tuple_get(struct py_object*, unsigned);
int py_tuple_set(struct py_object*, unsigned, struct py_object*);

void py_tuple_dealloc(struct py_object*);
int py_tuple_cmp(const struct py_object*, const struct py_object*);

struct py_object* py_tuple_cat(struct py_object*, struct py_object*);
struct py_object* py_tuple_ind(struct py_object*, unsigned);
struct py_object* py_tuple_slice(struct py_object*, unsigned, unsigned);

#endif
