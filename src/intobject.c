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

#define PY_INT_BLOCK_SIZE (1024) /* 1K less typical malloc overhead */
#define PY_INT_COUNT (PY_INT_BLOCK_SIZE / sizeof(struct py_int))

/* TODO: Python global state. */
static struct py_int* py_int_freelist = NULL;

static struct py_int* py_int_freelist_fill(void) {
	struct py_int* p;
	struct py_int* q;

	p = malloc(PY_INT_COUNT * sizeof(struct py_int));
	if(p == NULL) return (struct py_int*) py_error_set_nomem();

	q = p + PY_INT_COUNT;

	while(--q > p) *(struct py_int**) q = q - 1;

	*(struct py_int**) q = NULL;

	return p + PY_INT_COUNT - 1;
}

struct py_object* py_int_new(long ival) {
	struct py_int* v;

	if(py_int_freelist == NULL) {
		if((py_int_freelist = py_int_freelist_fill()) == NULL) return NULL;
	}

	v = py_int_freelist;
	py_int_freelist = *(struct py_int**) py_int_freelist;
	py_object_newref(v);

	v->ob.type = &py_int_type;
	v->value = ival;

	return (struct py_object*) v;
}

static void py_int_dealloc(struct py_object* v) {
	*(struct py_int**) v = py_int_freelist;
	py_int_freelist = (struct py_int*) v;
}

long py_int_get(const struct py_object* op) {
	if(!py_is_int(op)) {
		py_error_set_badcall();
		return -1;
	}
	else return ((struct py_int*) op)->value;
}

/* Methods */

static int py_int_cmp(const struct py_object* v, const struct py_object* w) {
	long i = py_int_get(v);
	long j = py_int_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}

struct py_type py_int_type = {
		{ 1, &py_type_type }, sizeof(struct py_int),
		py_int_dealloc, /* dealloc */
		py_int_cmp, /* cmp */
		0, /* sequencemethods */
};
