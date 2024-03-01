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

struct py_object* py_list_new(size)int size;
{
	int i;
	struct py_list* op;
	if(size < 0) {
		py_error_set_badcall();
		return NULL;
	}
	op = malloc(sizeof(struct py_list));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	if(size <= 0) {
		op->item = NULL;
	}
	else {
		op->item = malloc(size * sizeof(struct py_object*));
		if(op->item == NULL) {
			free(op);
			return py_error_set_nomem();
		}
	}
	PY_NEWREF(op);
	op->type = &py_list_type;
	op->size = size;
	for(i = 0; i < size; i++) {
		op->item[i] = NULL;
	}
	return (struct py_object*) op;
}

int py_list_size(op)struct py_object* op;
{
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return -1;
	}
	else {
		return ((struct py_list*) op)->size;
	}
}

struct py_object* py_list_get(op, i)struct py_object* op;
									int i;
{
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return NULL;
	}
	if(i < 0 || i >= (int) ((struct py_list*) op)->size) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}
	return ((struct py_list*) op)->item[i];
}

int py_list_set(op, i, newitem)struct py_object* op;
							   int i;
							   struct py_object* newitem;
{
	struct py_object* olditem;
	if(!py_is_list(op)) {
		if(newitem != NULL)
			PY_DECREF(newitem);
		py_error_set_badcall();
		return -1;
	}
	if(i < 0 || i >= (int) ((struct py_list*) op)->size) {
		if(newitem != NULL)
			PY_DECREF(newitem);
		py_error_set_string(
				PY_INDEX_ERROR, "list assignment index out of range");
		return -1;
	}
	olditem = ((struct py_list*) op)->item[i];
	((struct py_list*) op)->item[i] = newitem;
	if(olditem != NULL)
		PY_DECREF(olditem);
	return 0;
}

static int ins1(self, where, v)struct py_list* self;
							   int where;
							   struct py_object* v;
{
	int i;
	struct py_object** items;
	if(v == NULL) {
		py_error_set_badcall();
		return -1;
	}
	items = self->item;
	/* TODO: Leaky realloc. */
	items = realloc(items, (self->size + 1) * sizeof(struct py_object*));
	if(items == NULL) {
		py_error_set_nomem();
		return -1;
	}
	if(where < 0) {
		where = 0;
	}
	/* TODO: Address massive amount of `(int)' casting from old code. */
	if(where > (int) self->size) {
		where = (int) self->size;
	}
	for(i = (int) self->size; --i >= where;) {
		items[i + 1] = items[i];
	}
	PY_INCREF(v);
	items[where] = v;
	self->item = items;
	self->size++;
	return 0;
}

int py_list_insert(op, where, newitem)struct py_object* op;
									  int where;
									  struct py_object* newitem;
{
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return -1;
	}
	return ins1((struct py_list*) op, where, newitem);
}

int py_list_add(op, newitem)struct py_object* op;
							struct py_object* newitem;
{
	if(!py_is_list(op)) {
		py_error_set_badcall();
		return -1;
	}
	return ins1(
			(struct py_list*) op, (int) ((struct py_list*) op)->size, newitem);
}

/* Methods */

static void list_dealloc(op)struct py_list* op;
{
	int i;
	for(i = 0; i < (int) op->size; i++) {
		if(op->item[i] != NULL)
			PY_DECREF(op->item[i]);
	}
	if(op->item != NULL) {
		free(op->item);
	}
	free(op);
}

static void list_print(op, fp, flags)struct py_list* op;
									 FILE* fp;
									 int flags;
{
	int i;
	fprintf(fp, "[");
	for(i = 0; i < (int) op->size && !py_stop_print; i++) {
		if(i > 0) {
			fprintf(fp, ", ");
		}
		py_object_print(op->item[i], fp, flags);
	}
	fprintf(fp, "]");
}

struct py_object* list_repr(v)struct py_list* v;
{
	struct py_object* s, * t, * comma;
	int i;
	s = py_string_new("[");
	comma = py_string_new(", ");
	for(i = 0; i < (int) v->size && s != NULL; i++) {
		if(i > 0) {
			py_string_join(&s, comma);
		}
		t = py_object_repr(v->item[i]);
		py_string_join(&s, t);
		PY_DECREF(t);
	}
	PY_DECREF(comma);
	t = py_string_new("]");
	py_string_join(&s, t);
	PY_DECREF(t);
	return s;
}

static int list_compare(v, w)struct py_list* v, * w;
{
	int len = (v->size < w->size) ? v->size : w->size;
	int i;
	for(i = 0; i < len; i++) {
		int cmp = py_object_cmp(v->item[i], w->item[i]);
		if(cmp != 0) {
			return cmp;
		}
	}
	return v->size - w->size;
}

static int list_length(a)struct py_list* a;
{
	return a->size;
}

static struct py_object* list_item(a, i)struct py_list* a;
										int i;
{
	if(i < 0 || i >= (int) a->size) {
		py_error_set_string(PY_INDEX_ERROR, "list index out of range");
		return NULL;
	}
	PY_INCREF(a->item[i]);
	return a->item[i];
}

static struct py_object* list_slice(a, ilow, ihigh)struct py_list* a;
												   int ilow, ihigh;
{
	struct py_list* np;
	int i;
	if(ilow < 0) {
		ilow = 0;
	}
	else if(ilow > (int) a->size) {
		ilow = (int) a->size;
	}
	if(ihigh < 0) {
		ihigh = 0;
	}
	if(ihigh < ilow) {
		ihigh = ilow;
	}
	else if(ihigh > (int) a->size) {
		ihigh = (int) a->size;
	}
	np = (struct py_list*) py_list_new(ihigh - ilow);
	if(np == NULL) {
		return NULL;
	}
	for(i = ilow; i < ihigh; i++) {
		struct py_object* v = a->item[i];
		PY_INCREF(v);
		np->item[i - ilow] = v;
	}
	return (struct py_object*) np;
}

static struct py_object* list_concat(a, bb)struct py_list* a;
										   struct py_object* bb;
{
	int size;
	int i;
	struct py_list* np;
	struct py_list* b;
	if(!py_is_list(bb)) {
		py_error_set_badarg();
		return NULL;
	}
	b = ((struct py_list*) bb);
	size = a->size + b->size;
	np = (struct py_list*) py_list_new(size);
	if(np == NULL) {
		return py_error_set_nomem();
	}
	for(i = 0; i < (int) a->size; i++) {
		struct py_object* v = a->item[i];
		PY_INCREF(v);
		np->item[i] = v;
	}
	for(i = 0; i < (int) b->size; i++) {
		struct py_object* v = b->item[i];
		PY_INCREF(v);
		np->item[i + a->size] = v;
	}
	return (struct py_object*) np;
}

/* Added by Andrew Dalke, 27 March 2009 to handle the needed forward declaration for gcc*/
static int
list_ass_slice(struct py_object* a, int ilow, int ihigh, struct py_object* v);


static int list_ass_item(a, i, v)struct py_list* a;
								 int i;
								 struct py_object* v;
{
	if(i < 0 || i >= (int) a->size) {
		py_error_set_string(
				PY_INDEX_ERROR, "list assignment index out of range");
		return -1;
	}
	if(v == NULL) {
		return list_ass_slice((struct py_object*) a, i, i + 1, v);
	}
	PY_INCREF(v);
	PY_DECREF(a->item[i]);
	a->item[i] = v;
	return 0;
}

static int list_ass_slice(aa, ilow, ihigh, v)struct py_object* aa;
											 int ilow, ihigh;
											 struct py_object* v;
{
	struct py_object** item;
	struct py_list* a;
	struct py_list* b;
	int n; /* Size of replacement list */
	int d; /* Change in size */
	int k; /* Loop index */

	a = ((struct py_list*) aa);
	b = ((struct py_list*) v);
	if(v == NULL) {
		n = 0;
	}
	else if(py_is_list(v)) {
		n = b->size;
	}
	else {
		py_error_set_badarg();
		return -1;
	}
	if(ilow < 0) {
		ilow = 0;
	}
	else if(ilow > (int) a->size) {
		ilow = (int) a->size;
	}
	if(ihigh < 0) {
		ihigh = 0;
	}
	if(ihigh < ilow) {
		ihigh = ilow;
	}
	else if(ihigh > (int) a->size) {
		ihigh = (int) a->size;
	}
	item = a->item;
	d = n - (ihigh - ilow);
	if(d <= 0) { /* Delete -d items; PY_DECREF ihigh-ilow items */
		for(k = ilow; k < ihigh; k++)
			PY_DECREF(item[k]);
		if(d < 0) {
			for(/*k = ihigh*/; k < (int) a->size; k++) {
				item[k + d] = item[k];
			}
			a->size += d;
			/* TODO: Leaky realloc. */
			item = realloc(item, a->size * sizeof(struct py_object*));
			a->item = item;
		}
	}
	else { /* Insert d items; PY_DECREF ihigh-ilow items */
		/* TODO: Leaky realloc. */
		item = realloc(item, (a->size + d) * sizeof(struct py_object*));
		if(item == NULL) {
			py_error_set_nomem();
			return -1;
		}
		for(k = a->size; --k >= ihigh;) {
			item[k + d] = item[k];
		}
		for(/*k = ihigh-1*/; k >= ilow; --k)
			PY_DECREF(item[k]);
		a->item = item;
		a->size += d;
	}
	for(k = 0; k < n; k++, ilow++) {
		struct py_object* w = b->item[k];
		PY_INCREF(w);
		item[ilow] = w;
	}
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
	if(args == NULL || !py_is_tuple(args) || py_tuple_size(args) != 2) {
		py_error_set_badarg();
		return NULL;
	}
	if(!py_arg_int(py_tuple_get(args, 0), &i)) {
		return NULL;
	}
	return ins(self, i, py_tuple_get(args, 1));
}

static struct py_object* listappend(self, args)struct py_list* self;
											   struct py_object* args;
{
	return ins(self, (int) self->size, args);
}

static int cmp(v, w)char* v, * w;
{
	return py_object_cmp(*(struct py_object**) v, *(struct py_object**) w);
}

static struct py_object* listsort(self, args)struct py_list* self;
											 struct py_object* args;
{
	if(args != NULL) {
		py_error_set_badarg();
		return NULL;
	}
	py_error_clear();
	if(self->size > 1) {
		qsort(
				(char*) self->item, (int) self->size, sizeof(struct py_object*),
				cmp);
	}
	if(py_error_occurred()) {
		return NULL;
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

int py_list_sort(v)struct py_object* v;
{
	if(v == NULL || !py_is_list(v)) {
		py_error_set_badcall();
		return -1;
	}
	v = listsort((struct py_list*) v, (struct py_object*) NULL);
	if(v == NULL) {
		return -1;
	}
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
		list_length,    /*len*/
		list_concat,    /*cat*/
		0,              /*rep*/
		list_item,      /*ind*/
		list_slice,     /*slice*/
		list_ass_item,  /*assign_item*/
		list_ass_slice, /*assign_slice*/
};

struct py_type py_list_type = {
		PY_OB_SEQ_INIT(&py_type_type) 0, "list", sizeof(struct py_list), 0,
		list_dealloc,   /*dealloc*/
		list_print,     /*print*/
		list_getattr,   /*get_attr*/
		0,              /*set_attr*/
		list_compare,   /*cmp*/
		list_repr,      /*repr*/
		0,              /*numbermethods*/
		&list_as_sequence,  /*sequencemethods*/
		0,              /*mappingmethods*/
};
