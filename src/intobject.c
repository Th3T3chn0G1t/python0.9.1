/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Integer object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/intobject.h>
#include <python/stringobject.h>

/* Standard Booleans */
struct py_int py_true_object = { { 1, &py_int_type }, 1 };
struct py_int py_false_object = { { 1, &py_int_type }, 0 };

/* TODO: Use "weird pointers" for integers. */
/*
 * Integers are quite normal objects, to make object handling uniform.
 * (Using odd pointers to represent integers would save much space
 * but require extra checks for this special case throughout the code.)
 * Since, a typical Python program spends much of its time allocating
 * and deallocating integers, these operations should be very fast.
 * Therefore we use a dedicated allocation scheme with a much lower
 * overhead (in space and time) than straight malloc(): a simple
 * dedicated free list, filled when necessary with memory from malloc().
 */

#define BLOCK_SIZE (1024) /* 1K less typical malloc overhead */
#define N_INTOBJECTS (BLOCK_SIZE / sizeof(struct py_int))

static struct py_int* fill_free_list(void) {
	struct py_int* p;
	struct py_int* q;

	p = malloc(N_INTOBJECTS * sizeof(struct py_int));
	if(p == NULL) return (struct py_int*) py_error_set_nomem();

	q = p + N_INTOBJECTS;

	while(--q > p) *(struct py_int**) q = q - 1;

	*(struct py_int**) q = NULL;

	return p + N_INTOBJECTS - 1;
}

static struct py_int* free_list = NULL;

/* TODO: `int' and `double' for their respective object types. */
struct py_object* py_int_new(long ival) {
	struct py_int* v;
	if(free_list == NULL) {
		if((free_list = fill_free_list()) == NULL) {
			return NULL;
		}
	}
	v = free_list;
	free_list = *(struct py_int**) free_list;
	py_object_newref(v);
	v->ob.type = &py_int_type;
	v->value = ival;
	return (struct py_object*) v;
}

static void int_dealloc(v)struct py_int* v;
{
	*(struct py_int**) v = free_list;
	free_list = v;
}

long py_int_get(op)struct py_object* op;
{
	if(!py_is_int(op)) {
		py_error_set_badcall();
		return -1;
	}
	else {
		return ((struct py_int*) op)->value;
	}
}

/* Methods */

static int int_compare(v, w)struct py_int* v, * w;
{
	long i = v->value;
	long j = w->value;
	return (i < j) ? -1 : (i > j) ? 1 : 0;
}

struct py_type py_int_type = {
		{ 1, 0, &py_type_type }, sizeof(struct py_int),
		int_dealloc, /* dealloc */
		0, /* get_attr */
		int_compare, /* cmp */
		0, /* sequencemethods */
};
