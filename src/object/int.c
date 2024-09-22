/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Integer object implementation */

#include <python/std.h>

#include <python/object/int.h>
#include <python/object/string.h>

/* Standard Booleans */
struct py_int py_true_object = { { PY_TYPE_INT, 1 }, 1 };
struct py_int py_false_object = { { PY_TYPE_INT, 1 }, 0 };

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

static enum py_result py_int_freelist_fill(void) {
	struct py_int* p;
	struct py_int* q;

	if(!(p = calloc(PY_INT_COUNT, sizeof(struct py_int)))) {
		return PY_RESULT_OOM;
	}

	q = p + PY_INT_COUNT;

	while(--q > p) *(struct py_int**) q = q - 1;

	*(struct py_int**) q = 0;

	py_int_freelist = p + PY_INT_COUNT - 1;

	return PY_RESULT_OK;
}

struct py_object* py_int_new(py_value_t value) {
	struct py_int* v;

	if(!py_int_freelist && (py_int_freelist_fill() != PY_RESULT_OK)) return 0;

	v = py_int_freelist;
	py_int_freelist = *(struct py_int**) py_int_freelist;
	py_object_newref(v);

	v->ob.type = PY_TYPE_INT;
	v->value = value;

	return (void*) v;
}

void py_int_dealloc(struct py_object* v) {
	*(struct py_int**) v = py_int_freelist;
	py_int_freelist = (void*) v;
}

py_value_t py_int_get(const struct py_object* op) {
	return ((struct py_int*) op)->value;
}

/* Methods */

int py_int_cmp(const struct py_object* v, const struct py_object* w) {
	py_value_t i = py_int_get(v);
	py_value_t j = py_int_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}
