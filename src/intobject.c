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

static struct py_object* err_ovf(void) {
	py_error_set_string(PY_OVERFLOW_ERROR, "integer overflow");
	return NULL;
}

static struct py_object* err_zdiv(void) {
	py_error_set_string(PY_ZERO_DIVISION_ERROR, "integer division by zero");
	return NULL;
}

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

static struct py_object* int_add(v, w)struct py_int* v;
									  struct py_object* w;
{
	long a, b, x;
	if(!py_is_int(w)) {
		py_error_set_badarg();
		return NULL;
	}
	a = v->value;
	b = ((struct py_int*) w)->value;
	x = a + b;
	if((x ^ a) < 0 && (x ^ b) < 0) {
		return err_ovf();
	}
	return py_int_new(x);
}

static struct py_object* int_sub(v, w)struct py_int* v;
									  struct py_object* w;
{
	long a, b, x;
	if(!py_is_int(w)) {
		py_error_set_badarg();
		return NULL;
	}
	a = v->value;
	b = ((struct py_int*) w)->value;
	x = a - b;
	if((x ^ a) < 0 && (x ^ ~b) < 0) {
		return err_ovf();
	}
	return py_int_new(x);
}

static struct py_object* int_mul(v, w)struct py_int* v;
									  struct py_object* w;
{
	long a, b;
	/* double x; */
	if(!py_is_int(w)) {
		py_error_set_badarg();
		return NULL;
	}
	a = v->value;
	b = ((struct py_int*) w)->value;
	/* x = (double) a * (double) b; */

/* TODO: Need sensible overflow detection logic
       if (x > 0x7fffffff || x < (double) (long) 0x80000000)
               return err_ovf();
*/
	return py_int_new(a * b);
}

static struct py_object* int_div(v, w)struct py_int* v;
									  struct py_object* w;
{
	if(!py_is_int(w)) {
		py_error_set_badarg();
		return NULL;
	}
	if(((struct py_int*) w)->value == 0) {
		return err_zdiv();
	}
	return py_int_new(v->value / ((struct py_int*) w)->value);
}

static struct py_object* int_rem(v, w)struct py_int* v;
									  struct py_object* w;
{
	if(!py_is_int(w)) {
		py_error_set_badarg();
		return NULL;
	}
	if(((struct py_int*) w)->value == 0) {
		return err_zdiv();
	}
	return py_int_new(v->value % ((struct py_int*) w)->value);
}

static struct py_object* int_neg(v)struct py_int* v;
{
	long a, x;
	a = v->value;
	x = -a;
	if(a < 0 && x < 0) {
		return err_ovf();
	}
	return py_int_new(x);
}

static struct py_object* int_pos(struct py_object* v) {
	py_object_incref(v);
	return v;
}

static struct py_numbermethods int_as_number = {
		int_add, /* tp_add */
		int_sub, /* tp_sub */
		int_mul, /* tp_mul */
		int_div, /* tp_divide */
		int_rem, /* tp_remainder */
		int_neg, /* tp_negate */
		int_pos /* tp_plus */
};

struct py_type py_int_type = {
		{ 1, &py_type_type, 0 }, "int", sizeof(struct py_int),
		int_dealloc, /* dealloc */
		0, /* get_attr */
		0, /* set_attr */
		int_compare, /* cmp */
		&int_as_number, /* numbermethods */
		0, /* sequencemethods */
};
