/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Float object implementation */

/* XXX There should be overflow checks here, but it's hard to check
   for any kind of float exception without losing portability. */

#include <python/std.h>
#include <python/errors.h>

#include <python/floatobject.h>
#include <python/stringobject.h>

struct py_object* py_float_new(fval)double fval;
{
	/* For efficiency, this code is copied from py_object_new() */
	struct py_float* op = malloc(sizeof(struct py_float));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	PY_NEWREF(op);
	op->ob.type = &py_float_type;
	op->value = fval;
	return (struct py_object*) op;
}

double py_float_get(op)struct py_object* op;
{
	if(!py_is_float(op)) {
		py_error_set_badarg();
		return -1;
	}
	else {
		return ((struct py_float*) op)->value;
	}
}

/* Methods */

static void float_buf_repr(buf, v)char* buf;
								  struct py_float* v;
{
	char* cp;
	/* Subroutine for float_repr and float_print.
	   We want float numbers to be recognizable as such,
	   i.e., they should contain a decimal point or an exponent.
	   However, %g may print the number as an integer;
	   in such cases, we append ".0" to the string. */
	sprintf(buf, "%.12g", v->value);
	cp = buf;
	if(*cp == '-') {
		cp++;
	}
	for(; *cp != '\0'; cp++) {
		/* Any non-digit means it's not an integer;
		   this takes care of NAN and INF as well. */
		if(!isdigit(*cp)) {
			break;
		}
	}
	if(*cp == '\0') {
		*cp++ = '.';
		*cp++ = '0';
		*cp++ = '\0';
	}
}

static void float_print(v, fp, flags)struct py_float* v;
									 FILE* fp;
									 int flags;
{
	char buf[100];

	(void) flags;

	float_buf_repr(buf, v);
	fputs(buf, fp);
}

static struct py_object* float_repr(v)struct py_float* v;
{
	char buf[100];
	float_buf_repr(buf, v);
	return py_string_new(buf);
}

static int float_compare(v, w)struct py_float* v, * w;
{
	double i = v->value;
	double j = w->value;
	return (i < j) ? -1 : (i > j) ? 1 : 0;
}

static struct py_object* float_add(v, w)struct py_float* v;
										struct py_object* w;
{
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	return py_float_new(v->value + ((struct py_float*) w)->value);
}

static struct py_object* float_sub(v, w)struct py_float* v;
										struct py_object* w;
{
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	return py_float_new(v->value - ((struct py_float*) w)->value);
}

static struct py_object* float_mul(v, w)struct py_float* v;
										struct py_object* w;
{
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	return py_float_new(v->value * ((struct py_float*) w)->value);
}

static struct py_object* float_div(v, w)struct py_float* v;
										struct py_object* w;
{
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	if(((struct py_float*) w)->value == 0) {
		py_error_set_string(PY_ZERO_DIVISION_ERROR, "float division by zero");
		return NULL;
	}
	return py_float_new(v->value / ((struct py_float*) w)->value);
}

static struct py_object* float_rem(v, w)struct py_float* v;
										struct py_object* w;
{
	double wx;
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	wx = ((struct py_float*) w)->value;
	if(wx == 0.0) {
		py_error_set_string(PY_ZERO_DIVISION_ERROR, "float division by zero");
		return NULL;
	}
	return py_float_new(fmod(v->value, wx));
}

static struct py_object* float_pow(v, w)struct py_float* v;
										struct py_object* w;
{
	double iv, iw, ix;
	if(!py_is_float(w)) {
		py_error_set_badarg();
		return NULL;
	}
	iv = v->value;
	iw = ((struct py_float*) w)->value;
	if(iw == 0.0) {
		return py_float_new(1.0);
	} /* x**0 is always 1, even 0**0 */
	errno = 0;
	ix = pow(iv, iw);
	if(errno != 0) {
		/* XXX could it be another type of error? */
		py_error_set_errno(PY_OVERFLOW_ERROR);
		return NULL;
	}
	return py_float_new(ix);
}

static struct py_object* float_neg(v)struct py_float* v;
{
	return py_float_new(-v->value);
}

static struct py_object* float_pos(v)struct py_float* v;
{
	return py_float_new(v->value);
}

static struct py_numbermethods float_as_number = {
		float_add,      /*tp_add*/
		float_sub,      /*tp_sub*/
		float_mul,      /*tp_mul*/
		float_div,      /*tp_divide*/
		float_rem,      /*tp_remainder*/
		float_pow,      /*tp_power*/
		float_neg,      /*tp_negate*/
		float_pos,      /*tp_plus*/
};

struct py_type py_float_type = {
		{ 1, &py_type_type, 0 }, "float", sizeof(struct py_float), 0,
		py_object_delete, /* dealloc */
		float_print, /* print */
		0, /* get_attr */
		0, /* set_attr */
		float_compare, /* cmp */
		float_repr, /* repr */
		&float_as_number, /* numbermethods */
		0, /* sequencemethods */
		0, /* mappingmethods */
};

/*
XXX This is not enough. Need:
- automatic casts for mixed arithmetic (3.1 * 4)
- mixed comparisons (!)
- look at other uses of ints that could be extended to floats
*/
