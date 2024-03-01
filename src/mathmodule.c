/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Math module -- standard C math library functions, pi and e */

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/floatobject.h>
#include <python/intobject.h>
#include <python/tupleobject.h>
#include <python/dictobject.h>
#include <python/moduleobject.h>

static int getdoublearg(args, px)struct py_object* args;
								 double* px;
{
	if(args == NULL) {
		return py_error_set_badarg();
	}
	if(py_is_float(args)) {
		*px = py_float_get(args);
		return 1;
	}
	if(py_is_int(args)) {
		*px = py_int_get(args);
		return 1;
	}
	return py_error_set_badarg();
}

static int get2doublearg(args, px, py)struct py_object* args;
									  double* px, * py;
{
	if(args == NULL || !py_is_tuple(args) || py_tuple_size(args) != 2) {
		return py_error_set_badarg();
	}
	return getdoublearg(py_tuple_get(args, 0), px) &&
		   getdoublearg(py_tuple_get(args, 1), py);
}

static struct py_object* math_1(args, func)struct py_object* args;
								 double (* func)(double);
{
	double x;
	if(!getdoublearg(
			args, &x)) {
		return NULL;
	}
	errno = 0;
	x = (*func)(x);
	if(errno != 0) {
		return NULL;
	}
	else {
		return py_float_new(x);
	}
}

static struct py_object* math_2(args, func)struct py_object* args;
								 double (* func)(double, double);
{
	double x, y;
	if(!get2doublearg(
			args, &x, &y)) {
		return NULL;
	}
	errno = 0;
	x = (*func)(x, y);
	if(errno != 0) {
		return NULL;
	}
	else {
		return py_float_new(x);
	}
}

#define FUNC1(stubname, func) \
       static struct py_object* stubname(self, args) struct py_object*self, *args; { \
               (void) self; \
               return math_1(args, func); \
       }

#define FUNC2(stubname, func) \
       static struct py_object* stubname(self, args) struct py_object*self, *args; { \
               (void) self; \
               return math_2(args, func); \
       }

FUNC1(math_acos, acos)
FUNC1(math_asin, asin)
FUNC1(math_atan, atan)
FUNC2(math_atan2, atan2)
FUNC1(math_ceil, ceil)
FUNC1(math_cos, cos)
FUNC1(math_cosh, cosh)
FUNC1(math_exp, exp)
FUNC1(math_fabs, fabs)
FUNC1(math_floor, floor)
#if 0
/* XXX This one is not in the Amoeba library yet, so what the heck... */
FUNC2(math_fmod, fmod)
#endif
FUNC1(math_log, log)
FUNC1(math_log10, log10)
FUNC2(math_pow, pow)
FUNC1(math_sin, sin)
FUNC1(math_sinh, sinh)
FUNC1(math_sqrt, sqrt)
FUNC1(math_tan, tan)
FUNC1(math_tanh, tanh)

#if 0
/* What about these? */
double frexp(double x, int *i);
double ldexp(double x, int n);
double modf(double x, double *i);
#endif

static struct py_methodlist math_methods[] = {
		{ "acos", math_acos }, { "asin", math_asin }, { "atan", math_atan },
		{ "atan2", math_atan2 }, { "ceil", math_ceil }, { "cos", math_cos },
		{ "cosh", math_cosh }, { "exp", math_exp }, { "fabs", math_fabs },
		{ "floor", math_floor },
#if 0
		{"fmod", math_fmod},
		{"frexp", math_freqp},
		{"ldexp", math_ldexp},
#endif
		{ "log", math_log }, { "log10", math_log10 },
#if 0
		{"modf", math_modf},
#endif
		{ "pow", math_pow }, { "sin", math_sin }, { "sinh", math_sinh },
		{ "sqrt", math_sqrt }, { "tan", math_tan }, { "tanh", math_tanh },
		{ NULL, NULL }           /* sentinel */
};

void initmath() {
	struct py_object* m, * d, * v;

	m = py_module_init("math", math_methods);
	d = py_module_get_dict(m);
	py_dict_insert(d, "pi", v = py_float_new(atan(1.0) * 4.0));
	PY_DECREF(v);
	py_dict_insert(d, "e", v = py_float_new(exp(1.0)));
	PY_DECREF(v);
}
