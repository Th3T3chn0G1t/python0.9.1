/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Math module -- standard C math library functions */

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/floatobject.h>
#include <python/intobject.h>
#include <python/tupleobject.h>

static int py_arg_double(struct py_object* args, double* px) {
	if(args == NULL) return py_error_set_badarg();

	if(py_is_float(args)) {
		*px = py_float_get(args);
		return 1;
	}
	else if(py_is_int(args)) {
		*px = py_int_get(args);
		return 1;
	}

	return py_error_set_badarg();
}

static int py_arg_double_double(struct py_object* args, double* px, double* py) {
	if(args == NULL || !py_is_tuple(args) || py_varobject_size(args) != 2) {
		return py_error_set_badarg();
	}

	return py_arg_double(py_tuple_get(args, 0), px) &&
		   py_arg_double(py_tuple_get(args, 1), py);
}

static struct py_object* py_math1_impl(struct py_object* args, double (* func)(double)) {
	double x;

	if(!py_arg_double(args, &x)) return NULL;

	errno = 0;
	x = (*func)(x);

	if(errno != 0) return NULL;
	else return py_float_new(x);
}

static struct py_object* py_math2_impl(struct py_object* args, double (*func)(double, double)) {
	double x, y;

	if(!py_arg_double_double(args, &x, &y)) return NULL;

	errno = 0;
	x = (*func)(x, y);

	if(errno != 0) return NULL;
	else return py_float_new(x);
}

#define PY_MATH1(func) \
	static struct py_object* py_math_##func( \
			struct py_object* self, struct py_object* args) { \
		(void) self; \
		return py_math1_impl(args, func); \
	}

#define PY_MATH2(func) \
	static struct py_object* py_math_##func( \
			struct py_object* self, struct py_object* args) { \
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

void py_math_init(void) {
#define _(func) { #func, py_math_##func }
	static const struct py_methodlist math_methods[] = {
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
			{ NULL, NULL } /* sentinel */
	};
#undef _

	py_module_init("math", math_methods);
}
