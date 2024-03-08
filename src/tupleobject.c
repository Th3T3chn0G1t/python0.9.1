/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Tuple object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/tupleobject.h>
#include <python/stringobject.h>
#include <python/intobject.h>

struct py_object* py_tuple_new(unsigned size) {
	unsigned i;
	struct py_tuple* op;

	op = malloc(sizeof(struct py_tuple) + size * sizeof(struct py_object*));
	if(op == NULL) return py_error_set_nomem();

	py_object_newref(op);
	op->ob.type = &py_tuple_type;
	op->ob.size = size;

	for(i = 0; i < size; i++) op->item[i] = NULL;

	return (struct py_object*) op;
}

struct py_object* py_tuple_get(struct py_object* op, unsigned i) {
	if(!py_is_tuple(op)) {
		py_error_set_badcall();
		return NULL;
	}

	if(i >= py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "tuple index out of range");
		return NULL;
	}

	return ((struct py_tuple*) op)->item[i];
}

int py_tuple_set(struct py_object* op, unsigned i, struct py_object* newitem) {
	struct py_object* olditem;

	if(!py_is_tuple(op)) {
		if(newitem != NULL) py_object_decref(newitem);
		py_error_set_badcall();
		return -1;
	}

	if(i >= py_varobject_size(op)) {
		if(newitem != NULL) py_object_decref(newitem);
		py_error_set_string(
				PY_INDEX_ERROR, "tuple assignment index out of range");
		return -1;
	}
	olditem = ((struct py_tuple*) op)->item[i];
	((struct py_tuple*) op)->item[i] = newitem;
	if(olditem != NULL)
		py_object_decref(olditem);
	return 0;
}

/* Methods */

static void tupledealloc(struct py_object* op) {
	int i;
	for(i = 0; i < (int) py_varobject_size(op); i++) {
		if(((struct py_tuple*) op)->item[i] != NULL) {
			py_object_decref(((struct py_tuple*) op)->item[i]);
		}
	}
	free(op);
}

static int tuplecompare(const struct py_object* v, const struct py_object* w) {
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

static struct py_object* tupleitem(struct py_object* op, unsigned i) {
	struct py_object* item;

	if(i >= py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "tuple index out of range");
		return NULL;
	}

	item = ((struct py_tuple*) op)->item[i];

	py_object_incref(item);
	return item;
}

static struct py_object* tupleslice(
		struct py_object* op, unsigned ilow, unsigned ihigh) {

	struct py_tuple* np;
	unsigned i;

	if(ihigh > py_varobject_size(op)) ihigh = py_varobject_size(op);

	if(ihigh < ilow) ihigh = ilow;

	if(ilow == 0 && ihigh == py_varobject_size(op)) {
		/* XXX can only do this if tuples are immutable! */
		py_object_incref(op);
		return op;
	}

	np = (struct py_tuple*) py_tuple_new(ihigh - ilow);
	if(np == NULL) return NULL;

	for(i = ilow; i < ihigh; i++) {
		struct py_object* v = ((struct py_tuple*) op)->item[i];

		py_object_incref(v);
		np->item[i - ilow] = v;
	}

	return (struct py_object*) np;
}

static struct py_object* tupleconcat(
		struct py_object* a, struct py_object* b)  {

	unsigned size;
	unsigned i;
	struct py_tuple* np;

	if(!py_is_tuple(b)) {
		py_error_set_badarg();
		return NULL;
	}

	size = py_varobject_size(a) + py_varobject_size(b);

	np = (struct py_tuple*) py_tuple_new(size);
	if(np == NULL) return py_error_set_nomem();

	for(i = 0; i < py_varobject_size(a); i++) {
		struct py_object* v = ((struct py_tuple*) a)->item[i];

		py_object_incref(v);
		np->item[i] = v;
	}

	for(i = 0; i < py_varobject_size(b); i++) {
		struct py_object* v = ((struct py_tuple*) b)->item[i];

		py_object_incref(v);
		np->item[i + py_varobject_size(a)] = v;
	}

	return (struct py_object*) np;
}

static struct py_sequencemethods tuple_as_sequence = {
		tupleconcat, /* cat */
		tupleitem, /* ind */
		tupleslice, /* slice */
};

struct py_type py_tuple_type = {
		{ &py_type_type, 1 },
		sizeof(struct py_tuple) - sizeof(struct py_object*),
		tupledealloc, /* dealloc */
		tuplecompare,  /* cmp*/
		&tuple_as_sequence, /* sequencemethods */
};
