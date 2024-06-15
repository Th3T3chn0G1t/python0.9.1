/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Math module -- standard C math library functions */

#include <python/state.h>
#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/object/float.h>
#include <python/object/int.h>
#include <python/object/tuple.h>

typedef double (*py_math1_t)(double);
typedef double (*py_math2_t)(double, double);
typedef py_value_t (*py_math_val2_t)(py_value_t, py_value_t);

static int py_arg_double(struct py_object* args, double* px) {
	if(args == NULL) return py_error_set_badarg();

	if(args->type == PY_TYPE_FLOAT) {
		*px = py_float_get(args);
		return 1;
	}
	else if(args->type == PY_TYPE_INT) {
		*px = (double) py_int_get(args);
		return 1;
	}

	return py_error_set_badarg();
}

static int py_arg_double_double(
		struct py_object* args, double* px, double* py) {

	if(args == NULL || !(args->type == PY_TYPE_TUPLE) ||
		py_varobject_size(args) != 2) {

		return py_error_set_badarg();
	}

	return py_arg_double(py_tuple_get(args, 0), px) &&
		   py_arg_double(py_tuple_get(args, 1), py);
}

static struct py_object* py_math1_impl(
		struct py_object* args, py_math1_t func) {

	double x;

	if(!py_arg_double(args, &x)) return NULL;

	/* TODO: Better EH. */
	errno = 0;
	x = (*func)(x);

	if(errno != 0) return NULL;
	else return py_float_new(x);
}

static struct py_object* py_math2_impl(
		struct py_object* args, py_math2_t func) {

	double x, y;

	if(!py_arg_double_double(args, &x, &y)) return NULL;

	/* TODO: Better EH. */
	errno = 0;
	x = (*func)(x, y);

	if(errno != 0) return NULL;
	else return py_float_new(x);
}

#define PY_MATH1(func) \
	static struct py_object* py_math_##func( \
			struct py_env* env, \
			struct py_object* self, struct py_object* args) { \
		(void) env; \
		(void) self; \
		return py_math1_impl(args, func); \
	}

#define PY_MATH2(func) \
	static struct py_object* py_math_##func( \
			struct py_env* env, \
			struct py_object* self, struct py_object* args) { \
		(void) env; \
		(void) self; \
		return py_math2_impl(args, func); \
	}

PY_MATH1(acos)
PY_MATH1(asin)
PY_MATH1(atan)
PY_MATH1(ceil)
PY_MATH1(cos)
PY_MATH1(cosh)
PY_MATH1(exp)
PY_MATH1(fabs)
PY_MATH1(floor)
PY_MATH1(log)
PY_MATH1(log10)
PY_MATH1(sin)
PY_MATH1(sinh)
PY_MATH1(sqrt)
PY_MATH1(tan)
PY_MATH1(tanh)

PY_MATH2(atan2)
PY_MATH2(fmod)
PY_MATH2(pow)

static struct py_object* py_math_bitlist_op(
		struct py_object* args, py_math_val2_t func) {

	py_value_t res = 0;
	unsigned i;

	if(args->type != PY_TYPE_TUPLE) {
		py_error_set_badarg();
		return 0;
	}

	for(i = 0; i < py_varobject_size(args); ++i) {
		struct py_object* ob = py_tuple_get(args, i);
		py_value_t v = py_int_get(ob);

		res = func(res, v);
	}

	return py_int_new(res);
}


static py_value_t py_math_orbv(py_value_t v, py_value_t p) { return v | p; }
static struct py_object* py_math_orb(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	return py_math_bitlist_op(args, py_math_orbv);
}

static py_value_t py_math_andbv(py_value_t v, py_value_t p) { return v & p; }
static struct py_object* py_math_andb(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	return py_math_bitlist_op(args, py_math_andbv);
}

static py_value_t py_math_xorbv(py_value_t v, py_value_t p) { return v ^ p; }
static struct py_object* py_math_xorb(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	return py_math_bitlist_op(args, py_math_xorbv);
}

static struct py_object* py_math_val2(
		struct py_object* args, py_math_val2_t func) {

	struct py_object* a;
	struct py_object* b;

	if(args->type != PY_TYPE_TUPLE ||
		!(a = py_tuple_get(args, 0)) || a->type != PY_TYPE_INT ||
		!(b = py_tuple_get(args, 1)) || b->type != PY_TYPE_INT) {

		py_error_set_badarg();
		return 0;
	}

	return py_int_new(func(py_int_get(a), py_int_get(b)));
}

static py_value_t py_math_shlv(py_value_t v, py_value_t p) { return v << p; }
static struct py_object* py_math_shl(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	return py_math_val2(args, py_math_shlv);
}

static py_value_t py_math_shrv(py_value_t v, py_value_t p) { return v >> p; }
static struct py_object* py_math_shr(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	return py_math_val2(args, py_math_shrv);
}

static struct py_object* py_math_randf(
		struct py_env* env, struct py_object* self, struct py_object* args) {
	(void) env;
	(void) self;
	(void) args;
	return py_float_new((double) rand() / (double) RAND_MAX);
}

enum py_result py_math_init(struct py_env* env) {
#define _(func) { #func, py_math_##func }
	static const struct py_methodlist methods[] = {
			_(acos),
			_(asin),
			_(atan),
			_(atan2),
			_(ceil),
			_(cos),
			_(cosh),
			_(exp),
			_(fabs),
			_(floor),
			_(fmod),
			_(log),
			_(log10),
			_(pow),
			_(sin),
			_(sinh),
			_(sqrt),
			_(tan),
			_(tanh),
			_(orb),
			_(andb),
			_(xorb),
			_(shl),
			_(shr),
			_(randf),
			{ NULL, NULL } /* sentinel */
	};
#undef _

	if(!(py_module_new_methods(env, "math", methods))) return PY_RESULT_ERROR;

	return PY_RESULT_OK;
}
