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

	PY_NEWREF(op);
	op->ob.type = &py_list_type;
	op->ob.size = size;

	for(i = 0; i < size; i++) op->item[i] = NULL;

	return (struct py_object*) op;
}

struct py_object* py_list_get(struct py_object* op, int i) {
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return NULL;
	}

	if(i < 0 || i >= (int) py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}

	return ((struct py_list*) op)->item[i];
}

int py_list_set(struct py_object* op, int i, struct py_object* newitem) {
	struct py_object* olditem;
	struct py_list* lp = (struct py_list*) op;

	if(!py_is_list(op)) {
		if(newitem != NULL) PY_DECREF(newitem);

		py_error_set_badcall();
		return -1;
	}

	if(i < 0 || i >= (int) py_varobject_size(op)) {
		if(newitem != NULL) PY_DECREF(newitem);

		py_error_set_string(
				PY_INDEX_ERROR, "list assignment index out of range");
		return -1;
	}

	olditem = lp->item[i];
	lp->item[i] = newitem;

	if(olditem != NULL) PY_DECREF(olditem);

	return 0;
}

static int ins1(struct py_list* self, int where, struct py_object* v) {
	int i;
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

	if(where < 0) where = 0;

	/* TODO: Address massive amount of `(int)' casting from old code. */
	if(where > (int) self->ob.size) where = (int) self->ob.size;

	for(i = (int) self->ob.size; --i >= where;) items[i + 1] = items[i];

	PY_INCREF(v);
	items[where] = v;

	self->item = items;
	self->ob.size++;

	return 0;
}

int py_list_insert(
		struct py_object* op, int where, struct py_object* newitem) {

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
		if(lp->item[i] != NULL) PY_DECREF(lp->item[i]);
	}

	free(lp->item);
	free(op);
}

static void list_print(
		struct py_object* op, FILE* fp, enum py_print_mode mode) {

	struct py_list* lp = (struct py_list*) op;
	unsigned i;

	fprintf(fp, "[");

	for(i = 0; i < lp->ob.size; i++) {
		if(i > 0) fprintf(fp, ", ");
		py_object_print(lp->item[i], fp, mode);
	}

	fprintf(fp, "]");
}

struct py_object* list_repr(struct py_object* op) {
	struct py_list* lp = (struct py_list*) op;
	struct py_object* s;
	struct py_object* t;
	struct py_object* comma;
	unsigned i;

	s = py_string_new("[");
	comma = py_string_new(", ");
	for(i = 0; i < lp->ob.size && s != NULL; i++) {
		if(i > 0) py_string_join(&s, comma);

		t = py_object_repr(lp->item[i]);
		py_string_join(&s, t);
		PY_DECREF(t);
	}

	PY_DECREF(comma);

	t = py_string_new("]");
	py_string_join(&s, t);
	PY_DECREF(t);

	return s;
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

static struct py_object* list_item(struct py_object* op, int i) {
	struct py_object* item;

	if(i < 0 || i >= (int) py_varobject_size(op)) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}

	item = ((struct py_list*) op)->item[i];
	PY_INCREF(item);
	return item;
}

static struct py_object* list_slice(struct py_object* op, int ilow, int ihigh) {
	struct py_list* np;
	int i;

	if(ilow < 0) ilow = 0;
	else if(ilow > (int) py_varobject_size(op)) {
		ilow = (int) py_varobject_size(op);
	}

	if(ihigh < 0) ihigh = 0;

	if(ihigh < ilow) ihigh = ilow;
	else if(ihigh > (int) py_varobject_size(op)) {
		ihigh = (int) py_varobject_size(op);
	}

	np = (struct py_list*) py_list_new(ihigh - ilow);
	if(np == NULL) return NULL;

	for(i = ilow; i < ihigh; i++) {
		struct py_object* v = ((struct py_list*) op)->item[i];
		PY_INCREF(v);
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

		PY_INCREF(v);
		np->item[i] = v;
	}

	for(i = 0; i < py_varobject_size(b); i++) {
		struct py_object* v = ((struct py_list*) b)->item[i];
		PY_INCREF(v);
		np->item[i + py_varobject_size(a)] = v;
	}

	return (struct py_object*) np;
}



static int list_ass_slice(
		struct py_object* v, int ilow, int ihigh, struct py_object* w) {

	struct py_object** item;
	struct py_list* a;
	struct py_list* b;
	unsigned n; /* Size of replacement list */
	int d; /* Change in size */
	unsigned k; /* Loop index */

	a = ((struct py_list*) v);
	b = ((struct py_list*) w);

	if(w == NULL) n = 0;
	else if(py_is_list(w)) n = py_varobject_size(b);
	else {
		py_error_set_badarg();
		return -1;
	}

	if(ilow < 0) ilow = 0;
	else if(ilow > (int) py_varobject_size(a)) {
		ilow = (int) py_varobject_size(a);
	}

	if(ihigh < 0) ihigh = 0;

	if(ihigh < ilow) ihigh = ilow;
	else if(ihigh > (int) py_varobject_size(a)) {
		ihigh = (int) py_varobject_size(a);
	}

	item = a->item;
	d = (int) n - (ihigh - ilow);

	if(d <= 0) { /* Delete -d items; PY_DECREF ihigh-ilow items */
		for(k = ilow; k < (unsigned) ihigh; k++) PY_DECREF(item[k]);

		if(d < 0) {
			for(; k < py_varobject_size(a); k++) item[k + d] = item[k];

			((struct py_varobject*) a)->size += d;

			/* TODO: Leaky realloc. */
			item = realloc(
					item, py_varobject_size(a) * sizeof(struct py_object*));

			a->item = item;
		}
	}
	else { /* Insert d items; PY_DECREF ihigh-ilow items */
		/* TODO: Leaky realloc. */
		item = realloc(
				item, (py_varobject_size(a) + d) * sizeof(struct py_object*));
		if(item == NULL) {
			py_error_set_nomem();
			return -1;
		}

		for(k = py_varobject_size(a); --k >= (unsigned) ihigh;) {
			item[k + d] = item[k];
		}

		for(; k >= (unsigned) ilow; --k) PY_DECREF(item[k]);

		a->item = item;
		((struct py_varobject*) a)->size += d;
	}

	for(k = 0; k < n; k++, ilow++) {
		struct py_object* x = b->item[k];

		PY_INCREF(x);
		item[ilow] = x;
	}

	return 0;
}

static int list_ass_item(struct py_object* v, int i, struct py_object* w) {
	if(i < 0 || i >= (int) py_varobject_size(v)) {
		py_error_set_string(
				PY_INDEX_ERROR, "list assignment index out of range");
		return -1;
	}

	if(v == NULL) return list_ass_slice((struct py_object*) v, i, i + 1, w);

	PY_INCREF(w);
	PY_DECREF(((struct py_list*) v)->item[i]);
	((struct py_list*) v)->item[i] = w;

	return 0;
}

static struct py_object* ins(self, where, v)struct py_list* self;
											int where;
											struct py_object* v;
{
	if(ins1(self, where, v) != 0) {
		return NULL;
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_object* listinsert(self, args)struct py_list* self;
											   struct py_object* args;
{
	int i;
	if(args == NULL || !py_is_tuple(args) || py_varobject_size(args) != 2) {
		py_error_set_badarg();
		return NULL;
	}
	if(!py_arg_int(py_tuple_get(args, 0), &i)) {
		return NULL;
	}
	return ins(self, i, py_tuple_get(args, 1));
}

static struct py_object* listappend(
		struct py_object* self, struct py_object* args) {

	return ins((struct py_list*) self, (int) py_varobject_size(self), args);
}

static int py_list_cmp_qsort(
		const void* v, const void* w) {

	return py_object_cmp(*(struct py_object**) v, *(struct py_object**) w);
}

static struct py_object* listsort(
		struct py_object* self, struct py_object* args) {

	if(args != NULL) {
		py_error_set_badarg();
		return NULL;
	}

	py_error_clear();
	if(py_varobject_size(self) > 1) {
		int (*cmp)(const void*, const void*) = py_list_cmp_qsort;
		struct py_object** item = ((struct py_list*) self)->item;

		qsort(item, py_varobject_size(self), sizeof(struct py_object*), cmp);
	}

	if(py_error_occurred()) return NULL;

	PY_INCREF(PY_NONE);
	return PY_NONE;
}

int py_list_sort(struct py_object* v) {
	if(v == NULL || !py_is_list(v)) {
		py_error_set_badcall();
		return -1;
	}

	v = listsort(v, (struct py_object*) NULL);
	if(v == NULL) return -1;

	PY_DECREF(v);
	return 0;
}

static struct py_methodlist list_methods[] = {
		{ "append", listappend },
		{ "insert", listinsert },
		{ "sort",   listsort },
		{ NULL, NULL }           /* sentinel */
};

static struct py_object* list_getattr(f, name)struct py_list* f;
											  char* name;
{
	return py_methodlist_find(list_methods, (struct py_object*) f, name);
}

static struct py_sequencemethods list_as_sequence = {
		list_concat,    /*cat*/
		0,              /*rep*/
		list_item,      /*ind*/
		list_slice,     /*slice*/
		list_ass_item,  /*assign_item*/
		list_ass_slice, /*assign_slice*/
};

struct py_type py_list_type = {
		{ 1, &py_type_type, 0 }, "list", sizeof(struct py_list), 0,
		list_dealloc, /* dealloc */
		list_print, /* print */
		list_getattr, /* get_attr */
		0, /* set_attr */
		list_compare, /* cmp */
		list_repr, /* repr */
		0, /* numbermethods */
		&list_as_sequence, /* sequencemethods */
		0, /* mappingmethods */
};
