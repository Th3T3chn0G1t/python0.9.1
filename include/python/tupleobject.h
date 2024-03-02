/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Tuple object interface */

#ifndef PY_TUPLEOBJECT_H
#define PY_TUPLEOBJECT_H

/*
 * Another generally useful object type is an tuple of object pointers.
 * This is a mutable type: the tuple items can be changed (but not their
 * number). Out-of-range indices or non-tuple objects are ignored.
 *
 * *** WARNING *** py_tuple_set does not increment the new item's reference
 * count, but does decrement the reference count of the item it replaces,
 * if not nil. It does *decrement* the reference count if it is *not*
 * inserted in the tuple. Similarly, py_tuple_get does not increment the
 * returned item's reference count.
 */

struct py_tuple {
	struct py_varobject ob;
	struct py_object* item[1];
};

/* TODO: Python global state. */
extern struct py_type py_tuple_type;

#define py_is_tuple(op) ((op)->type == &py_tuple_type)

struct py_object* py_tuple_new(int size);
struct py_object* py_tuple_get(struct py_object*, int);
int py_tuple_set(struct py_object*, int, struct py_object*);

#endif
