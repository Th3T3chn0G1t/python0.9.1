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

struct py_object* py_tuple_new(size)int size;
{
	int i;
	struct py_tuple* op;
	if(size < 0) {
		py_error_set_badcall();
		return NULL;
	}
	op = malloc(sizeof(struct py_tuple) + size * sizeof(struct py_object*));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	PY_NEWREF(op);
	op->type = &py_tuple_type;
	op->size = size;
	for(i = 0; i < size; i++) {
		op->item[i] = NULL;
	}
	return (struct py_object*) op;
}

int py_tuple_size(op)struct py_object* op;
{
	if(!py_is_tuple(op)) {
		py_error_set_badcall();
		return -1;
	}
	else {
		return ((struct py_tuple*) op)->size;
	}
}

struct py_object* py_tuple_get(op, i)struct py_object* op;
									 int i;
{
	if(!py_is_tuple(op)) {
		py_error_set_badcall();
		return NULL;
	}
	if(i < 0 || i >= (int) ((struct py_tuple*) op)->size) {
		py_error_set_string(PY_INDEX_ERROR, "tuple index out of range");
		return NULL;
	}
	return ((struct py_tuple*) op)->item[i];
}

int py_tuple_set(op, i, newitem)struct py_object* op;
								int i;
								struct py_object* newitem;
{
	struct py_object* olditem;

	if(!py_is_tuple(op)) {
		if(newitem != NULL) PY_DECREF(newitem);
		py_error_set_badcall();
		return -1;
	}

	if(i < 0 || i >= (int) ((struct py_tuple*) op)->size) {
		if(newitem != NULL) PY_DECREF(newitem);
		py_error_set_string(
				PY_INDEX_ERROR, "tuple assignment index out of range");
		return -1;
	}
	olditem = ((struct py_tuple*) op)->item[i];
	((struct py_tuple*) op)->item[i] = newitem;
	if(olditem != NULL)
		PY_DECREF(olditem);
	return 0;
}

/* Methods */

static void tupledealloc(op)struct py_tuple* op;
{
	int i;
	for(i = 0; i < (int) op->size; i++) {
		if(op->item[i] != NULL)
			PY_DECREF(op->item[i]);
	}
	free(op);
}

static void tupleprint(op, fp, flags)struct py_tuple* op;
									 FILE* fp;
									 int flags;
{
	int i;
	fprintf(fp, "(");
	for(i = 0; i < (int) op->size; i++) {
		if(i > 0) {
			fprintf(fp, ", ");
		}
		py_object_print(op->item[i], fp, flags);
	}
	if(op->size == 1) {
		fprintf(fp, ",");
	}
	fprintf(fp, ")");
}

struct py_object* tuplerepr(v)struct py_tuple* v;
{
	struct py_object* s, * t, * comma;
	int i;
	s = py_string_new("(");
	comma = py_string_new(", ");
	for(i = 0; i < (int) v->size && s != NULL; i++) {
		if(i > 0) {
			py_string_join(&s, comma);
		}
		t = py_object_repr(v->item[i]);
		py_string_join(&s, t);
		if(t != NULL)
			PY_DECREF(t);
	}
	PY_DECREF(comma);
	if(v->size == 1) {
		t = py_string_new(",");
		py_string_join(&s, t);
		PY_DECREF(t);
	}
	t = py_string_new(")");
	py_string_join(&s, t);
	PY_DECREF(t);
	return s;
}

static int tuplecompare(v, w)struct py_tuple* v, * w;
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

static int tuplelength(a)struct py_tuple* a;
{
	return a->size;
}

static struct py_object* tupleitem(a, i)struct py_tuple* a;
										int i;
{
	if(i < 0 || i >= (int) a->size) {
		py_error_set_string(PY_INDEX_ERROR, "tuple index out of range");
		return NULL;
	}
	PY_INCREF(a->item[i]);
	return a->item[i];
}

static struct py_object* tupleslice(a, ilow, ihigh)struct py_tuple* a;
												   int ilow, ihigh;
{
	struct py_tuple* np;
	int i;
	if(ilow < 0) {
		ilow = 0;
	}
	if(ihigh > (int) a->size) {
		ihigh = (int) a->size;
	}
	if(ihigh < ilow) {
		ihigh = ilow;
	}
	if(ilow == 0 && ihigh == (int) a->size) {
		/* XXX can only do this if tuples are immutable! */
		PY_INCREF(a);
		return (struct py_object*) a;
	}
	np = (struct py_tuple*) py_tuple_new(ihigh - ilow);
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

static struct py_object* tupleconcat(a, bb)struct py_tuple* a;
										   struct py_object* bb;
{
	int size;
	int i;
	struct py_tuple* np;
	struct py_tuple* b = (struct py_tuple*) bb;

	if(!py_is_tuple(bb)) {
		py_error_set_badarg();
		return NULL;
	}

	size = (int) (a->size + b->size);
	np = (struct py_tuple*) py_tuple_new(size);
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

static struct py_sequencemethods tuple_as_sequence = {
		tuplelength,    /*len*/
		tupleconcat,    /*cat*/
		0,              /*rep*/
		tupleitem,      /*ind*/
		tupleslice,     /*slice*/
		0,              /*assign_item*/
		0,              /*assign_slice*/
};

struct py_type py_tuple_type = {
		PY_OB_SEQ_INIT(&py_type_type) 0, "tuple",
		sizeof(struct py_tuple) - sizeof(struct py_object*),
		sizeof(struct py_object*), tupledealloc,   /*dealloc*/
		tupleprint,     /*print*/
		0,              /*get_attr*/
		0,              /*set_attr*/
		tuplecompare,   /*cmp*/
		tuplerepr,      /*repr*/
		0,              /*numbermethods*/
		&tuple_as_sequence, /*sequencemethods*/
		0,              /*mappingmethods*/
};
