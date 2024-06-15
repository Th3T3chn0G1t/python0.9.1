/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Tuple object implementation */

#include <python/std.h>

#include <python/object/tuple.h>

struct py_object* py_tuple_new(unsigned size) {
	struct py_tuple* op;

	op = calloc(1, sizeof(struct py_tuple) + size * sizeof(struct py_object*));
	if(!op) return 0;

	py_object_newref(op);
	op->ob.type = PY_TYPE_TUPLE;
	op->ob.size = size;

	return (void*) op;
}

struct py_object* py_tuple_get(const struct py_object* op, unsigned i) {
	return ((struct py_tuple*) op)->item[i];
}

void py_tuple_set(struct py_object* op, unsigned i, struct py_object* item) {
	struct py_tuple* tp = (void*) op;

	py_object_decref(tp->item[i]);
	tp->item[i] = item;
}

/* Methods */

void py_tuple_dealloc(struct py_object* op) {
	unsigned i;

	for(i = 0; i < py_varobject_size(op); i++) {
		py_object_decref(((struct py_tuple*) op)->item[i]);
	}

	free(op);
}

int py_tuple_cmp(const struct py_object* v, const struct py_object* w) {
	unsigned a, b;
	unsigned len;
	unsigned i;

	a = py_varobject_size(v);
	b = py_varobject_size(w);
	len = (a < b) ? a : b;

	for(i = 0; i < len; i++) {
		struct py_object* x = ((struct py_tuple*) v)->item[i];
		struct py_object* y = ((struct py_tuple*) w)->item[i];

		int cmp = py_object_cmp(x, y);
		if(cmp != 0) return cmp;
	}

	return (int) (a - b);
}

struct py_object* py_tuple_ind(struct py_object* op, unsigned i) {
	return py_object_incref(((struct py_tuple*) op)->item[i]);
}

struct py_object* py_tuple_slice(
		struct py_object* op, unsigned low, unsigned high) {

	struct py_tuple* np;
	unsigned i;
	unsigned size = py_varobject_size(op);

	if(high > size) high = size;
	if(high < low) high = low;

	/* TODO: can only do this if tuples are immutable! */
	if(low == 0 && high == size) return py_object_incref(op);

	if(!(np = (void*) py_tuple_new(high - low))) return 0;

	for(i = low; i < high; i++) {
		np->item[i - low] = py_object_incref(((struct py_tuple*) op)->item[i]);
	}

	return (void*) np;
}

struct py_object* py_tuple_cat(struct py_object* a, struct py_object* b)  {
	unsigned i;
	struct py_tuple* np;
	struct py_object* v;

	unsigned sz_a = py_varobject_size(a);
	unsigned sz_b = py_varobject_size(b);
	unsigned size = sz_a + sz_b;

	if(!(np = (void*) py_tuple_new(size))) return 0;

	for(i = 0; i < sz_a; i++) {
		np->item[i] = py_object_incref(((struct py_tuple*) a)->item[i]);
	}

	for(i = 0; i < sz_b; i++) {
		v = py_object_incref(((struct py_tuple*) b)->item[i]);
		np->item[i + py_varobject_size(a)] = v;
	}

	return (void*) np;
}
