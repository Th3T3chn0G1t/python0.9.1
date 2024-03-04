/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* List object implementation */

#include <python/std.h>
#include <python/errors.h>
#include <python/modsupport.h>

#include <python/listobject.h>
#include <python/stringobject.h>
#include <python/tupleobject.h>

struct py_object* py_list_new(unsigned size) {
	unsigned i;
	struct py_list* op;

	op = malloc(sizeof(struct py_list));
	if(op == NULL) return py_error_set_nomem();

	if(size <= 0) op->item = NULL;
	else {
		op->item = malloc(size * sizeof(struct py_object*));
		if(op->item == NULL) {
			free(op);
			return py_error_set_nomem();
		}
	}

	py_object_newref(op);
	op->ob.type = &py_list_type;
	op->ob.size = size;

	for(i = 0; i < size; i++) op->item[i] = NULL;

	return (struct py_object*) op;
}

struct py_object* py_list_get(struct py_object* op, unsigned i) {
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return NULL;
	}

	if(i >= py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}

	return ((struct py_list*) op)->item[i];
}

int py_list_set(struct py_object* op, unsigned i, struct py_object* newitem) {
	struct py_object* olditem;
	struct py_list* lp = (struct py_list*) op;

	if(!py_is_list(op)) {
		if(newitem != NULL) py_object_decref(newitem);

		py_error_set_badcall();
		return -1;
	}

	if(i >= py_varobject_size(op)) {
		if(newitem != NULL) py_object_decref(newitem);

		py_error_set_string(
				PY_INDEX_ERROR, "list assignment index out of range");
		return -1;
	}

	olditem = lp->item[i];
	lp->item[i] = newitem;

	py_object_decref(olditem);

	return 0;
}

static int ins1(struct py_list* self, unsigned where, struct py_object* v) {
	struct py_object** items;

	if(v == NULL) {
		py_error_set_badcall();
		return -1;
	}

	items = self->item;
	/* TODO: Leaky realloc. */
	items = realloc(items, (self->ob.size + 1) * sizeof(struct py_object*));

	if(items == NULL) {
		py_error_set_nomem();
		return -1;
	}

	if(where > self->ob.size) where = self->ob.size;

	memmove(
			&items[where + 1], &items[where],
			(self->ob.size - where) * sizeof(struct py_object*));
	py_object_incref(v);
	items[where] = v;

	self->item = items;
	self->ob.size++;

	return 0;
}

int py_list_insert(
		struct py_object* op, unsigned where, struct py_object* newitem) {

	if(!py_is_list(op)) {
		py_error_set_badcall();
		return -1;
	}

	return ins1((struct py_list*) op, where, newitem);
}

int py_list_add(struct py_object* op, struct py_object* newitem) {
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return -1;
	}

	return ins1((struct py_list*) op, (int) py_varobject_size(op), newitem);
}

/* Methods */
static void list_dealloc(struct py_object* op) {
	struct py_list* lp = (struct py_list*) op;
	unsigned i;

	for(i = 0; i < lp->ob.size; i++) {
		if(lp->item[i] != NULL) py_object_decref(lp->item[i]);
	}

	free(lp->item);
}

static int list_compare(const struct py_object* v, const struct py_object* w) {
	unsigned a = py_varobject_size(v);
	unsigned b = py_varobject_size(w);
	unsigned len = (a < b) ? a : b;
	unsigned i;

	for(i = 0; i < len; i++) {
		struct py_object* x = ((struct py_list*) v)->item[i];
		struct py_object* y = ((struct py_list*) w)->item[i];

		int cmp = py_object_cmp(x, y);
		if(cmp != 0) return cmp;
	}

	return (int) (a - b);
}

static struct py_object* list_item(struct py_object* op, unsigned i) {
	struct py_object* item;

	if(i >= py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}

	item = ((struct py_list*) op)->item[i];
	py_object_incref(item);
	return item;
}

static struct py_object* list_slice(
		struct py_object* op, unsigned ilow, unsigned ihigh) {

	struct py_list* np;
	unsigned i;

	if(ilow > py_varobject_size(op)) ilow = py_varobject_size(op);

	if(ihigh < ilow) ihigh = ilow;
	else if(ihigh > py_varobject_size(op)) ihigh = py_varobject_size(op);

	np = (struct py_list*) py_list_new(ihigh - ilow);
	if(np == NULL) return NULL;

	for(i = ilow; i < ihigh; i++) {
		struct py_object* v = ((struct py_list*) op)->item[i];
		py_object_incref(v);
		np->item[i - ilow] = v;
	}

	return (struct py_object*) np;
}

static struct py_object* list_concat(
		struct py_object* a, struct py_object* b) {

	unsigned size;
	unsigned i;
	struct py_list* np;

	if(!py_is_list(b)) {
		py_error_set_badarg();
		return NULL;
	}

	size = py_varobject_size(a) + py_varobject_size(b);

	np = (struct py_list*) py_list_new(size);
	if(np == NULL) return py_error_set_nomem();

	for(i = 0; i < py_varobject_size(a); i++) {
		struct py_object* v = ((struct py_list*) a)->item[i];

		py_object_incref(v);
		np->item[i] = v;
	}

	for(i = 0; i < py_varobject_size(b); i++) {
		struct py_object* v = ((struct py_list*) b)->item[i];
		py_object_incref(v);
		np->item[i + py_varobject_size(a)] = v;
	}

	return (struct py_object*) np;
}

static struct py_sequencemethods list_as_sequence = {
		list_concat, /* cat */
		list_item, /* ind */
		list_slice /* slice */
};

struct py_type py_list_type = {
		{ 1, &py_type_type }, sizeof(struct py_list),
		list_dealloc, /* dealloc */
		list_compare, /* cmp */
		&list_as_sequence, /* sequencemethods */
};
