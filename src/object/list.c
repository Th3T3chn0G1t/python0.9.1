/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* List object implementation */

#include <python/std.h>

#include <python/object/list.h>

struct py_object* py_list_new(unsigned size) {
	struct py_list* op;

	if(!(op = py_object_new(PY_TYPE_LIST))) return 0;
	op->ob.size = size;

	if(!(op->item = calloc(size, sizeof(struct py_object*)))) {
		free(op);
		return 0;
	}

	return (void*) op;
}

struct py_object* py_list_get(const struct py_object* op, unsigned i) {
	return ((struct py_list*) op)->item[i];
}

void py_list_set(struct py_object* op, unsigned i, struct py_object* item) {
	struct py_object* old;
	struct py_list* lp = (void*) op;

	old = lp->item[i];
	lp->item[i] = item;

	py_object_decref(old);
}

static int py_list_insert_impl(
		struct py_list* self, unsigned where, struct py_object* v) {

	struct py_object** items;

	/* This isn't leaky -- we want to preserve original in OOM case here. */
	items = realloc(self->item, (self->ob.size + 1) * sizeof(struct py_object*));
	if(!items) return -1;

	if(where > self->ob.size) where = self->ob.size;

	memmove(
			&items[where + 1], &items[where],
			(self->ob.size - where) * sizeof(struct py_object*));

	items[where] = py_object_incref(v);

	self->item = items;
	self->ob.size++;

	return 0;
}

int py_list_insert(
		struct py_object* op, unsigned where, struct py_object* item) {

	return py_list_insert_impl((void*) op, where, item);
}

int py_list_add(struct py_object* op, struct py_object* item) {
	return py_list_insert_impl((void*) op, py_varobject_size(op), item);
}

/* Methods */
void py_list_dealloc(struct py_object* op) {
	unsigned i;
	struct py_list* lp = (void*) op;

	for(i = 0; i < lp->ob.size; i++) py_object_decref(lp->item[i]);

	free(lp->item);
}

int py_list_cmp(const struct py_object* v, const struct py_object* w) {
	unsigned i;
	unsigned a = py_varobject_size(v);
	unsigned b = py_varobject_size(w);
	unsigned len = (a < b) ? a : b;

	for(i = 0; i < len; i++) {
		struct py_object* x = ((struct py_list*) v)->item[i];
		struct py_object* y = ((struct py_list*) w)->item[i];

		int cmp = py_object_cmp(x, y);
		if(cmp) return cmp;
	}

	return (int) (a - b);
}

struct py_object* py_list_ind(struct py_object* op, unsigned i) {
	return py_object_incref(((struct py_list*) op)->item[i]);
}

struct py_object* py_list_slice(
		struct py_object* op, unsigned low, unsigned high) {

	struct py_list* np;
	unsigned i;

	if(low > py_varobject_size(op)) low = py_varobject_size(op);

	if(high < low) high = low;
	else if(high > py_varobject_size(op)) high = py_varobject_size(op);

	if(!(np = (void*) py_list_new(high - low))) return 0;

	for(i = low; i < high; i++) {
		struct py_object* v = ((struct py_list*) op)->item[i];
		np->item[i - low] = py_object_incref(v);
	}

	return (struct py_object*) np;
}

struct py_object* py_list_cat(struct py_object* a, struct py_object* b) {
	unsigned i;
	unsigned sz_a = py_varobject_size(a);
	unsigned sz_b = py_varobject_size(b);
	struct py_list* np;

	if(!(np = (void*) py_list_new(sz_a + sz_b))) return 0;

	for(i = 0; i < sz_a; i++) {
		np->item[i] = py_object_incref(((struct py_list*) a)->item[i]);
	}

	for(i = 0; i < sz_b; i++) {
		np->item[i + sz_a] = py_object_incref(((struct py_list*) b)->item[i]);
	}

	return (void*) np;
}
