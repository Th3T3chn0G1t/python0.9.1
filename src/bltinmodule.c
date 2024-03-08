/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Built-in functions */

#include <python/node.h>
#include <python/graminit.h>
#include <python/import.h>
#include <python/modsupport.h>
#include <python/result.h>
#include <python/ceval.h>
#include <python/errors.h>

#include <python/bltinmodule.h>

#include <python/intobject.h>
#include <python/floatobject.h>
#include <python/stringobject.h>
#include <python/moduleobject.h>
#include <python/listobject.h>
#include <python/dictobject.h>
#include <python/tupleobject.h>

/* Predefined exceptions */

struct py_object* py_runtime_error;
struct py_object* py_eof_error;
struct py_object* py_type_error;
struct py_object* py_memory_error;
struct py_object* py_name_error;
struct py_object* py_system_error;

int py_is_sequence(const void* op) {
	return py_is_list(op) || py_is_tuple(op) || py_is_string(op);
}

static struct py_object* builtin_float(
		struct py_object* self, struct py_object* v) {

	(void) self;

	if(v == NULL) {
		/* */
	}
	else if(py_is_float(v)) {
		py_object_incref(v);
		return v;
	}
	else if(py_is_int(v)) {
		long x = py_int_get(v);
		return py_float_new((double) x);
	}

	py_error_set_string(py_type_error, "float() argument must be float or int");
	return NULL;
}

static struct py_object* builtin_int(
		struct py_object* self, struct py_object* v) {

	(void) self;

	if(v == NULL) {
		/* */
	}
	else if(py_is_int(v)) {
		py_object_incref(v);
		return v;
	}
	else if(py_is_float(v)) {
		double x = py_float_get(v);
		return py_int_new((long) x);
	}

	py_error_set_string(py_type_error, "int() argument must be float or int");
	return NULL;
}

static struct py_object* builtin_len(
		struct py_object* self, struct py_object* v) {

	long len;

	(void) self;

	if(v == NULL) {
		py_error_set_string(py_type_error, "len() without argument");
		return NULL;
	}

	if(py_is_sequence(v)) len = py_varobject_size(v);
	else if(py_is_dict(v)) len = ((struct py_dict*) v)->used;
	else {
		py_error_set_string(py_type_error, "len() of unsized object");
		return NULL;
	}

	return py_int_new(len);
}

static struct py_object* builtin_range(
		struct py_object* self, struct py_object* v) {

	static char* errmsg = "range() requires 1-3 int arguments";
	int i, n;
	long ilow, ihigh, istep;

	(void) self;

	if(v != NULL && py_is_int(v)) {
		ilow = 0;
		ihigh = py_int_get(v);
		istep = 1;
	}
	else if(v == NULL || !py_is_tuple(v)) {
		py_error_set_string(py_type_error, errmsg);
		return NULL;
	}
	else {
		n = py_varobject_size(v);

		if(n < 1 || n > 3) {
			py_error_set_string(py_type_error, errmsg);
			return NULL;
		}

		for(i = 0; i < n; i++) {
			if(!py_is_int(py_tuple_get(v, i))) {
				py_error_set_string(py_type_error, errmsg);
				return NULL;
			}
		}

		if(n == 3) {
			istep = py_int_get(py_tuple_get(v, 2));
			--n;
		}
		else { istep = 1; }

		ihigh = py_int_get(py_tuple_get(v, --n));

		if(n > 0) { ilow = py_int_get(py_tuple_get(v, 0)); }
		else { ilow = 0; }
	}

	if(istep == 0) {
		py_error_set_string(py_runtime_error, "zero step for range()");
		return NULL;
	}

	/* XXX ought to check overflow of subion */
	if(istep > 0) { n = (ihigh - ilow + istep - 1) / istep; }
	else { n = (ihigh - ilow + istep + 1) / istep; }

	if(n < 0) n = 0;

	v = py_list_new(n);
	if(v == NULL) return NULL;

	for(i = 0; i < n; i++) {
		struct py_object* w = py_int_new(ilow);

		if(w == NULL) {
			py_object_decref(v);
			return NULL;
		}

		py_list_set(v, i, w);
		ilow += istep;
	}

	return v;
}

static struct py_object* builtin_append(
		struct py_object* self, struct py_object* v) {

	struct py_object* lp = py_tuple_get(v, 0);
	struct py_object* op = py_tuple_get(v, 1);

	(void) self;

	if(!lp || !op || py_list_add(lp, op) == -1) return 0;

	return py_object_incref(PY_NONE);
}

static struct py_object* builtin_insert(
		struct py_object* self, struct py_object* args) {

	struct py_object* lp;
	struct py_object* ind;
	struct py_object* op;

	(void) self;

	if(!args || !py_is_tuple(args) || py_varobject_size(args) != 2 ||
			!(lp = py_tuple_get(args, 0)) || !py_is_list(lp) ||
			!(ind = py_tuple_get(args, 1)) || !py_is_int(ind) ||
			!(op = py_tuple_get(args, 2))) {

		py_error_set_badarg();
		return NULL;
	}

	if(py_list_insert(lp, py_int_get(ind), op) == -1) return NULL;

	return py_object_incref(PY_NONE);
}

static struct py_object* builtin_pass(
		struct py_object* self, struct py_object* args) {

	(void) self;
	(void) args;

	return py_object_incref(PY_NONE);
}

static struct py_methodlist builtin_methods[] = {
		{ "float", builtin_float },
		{ "int", builtin_int },
		{ "len", builtin_len },
		{ "range", builtin_range },
		{ "append", builtin_append },
		{ "insert", builtin_insert },
		{ "pass", builtin_pass },
		{ NULL, NULL } };

/* TODO: Python global state. */
static struct py_object* builtin_dict;

struct py_object* py_builtin_get(const char* name) {
	return py_dict_lookup(builtin_dict, name);
}

static struct py_object* newstdexception(char* name, char* message) {
	struct py_object* v = py_string_new(message);

	if(v == NULL || py_dict_insert(builtin_dict, name, v) != 0) {
		py_fatal("no mem for new standard exception");
	}

	return v;
}

static void initerrors(void) {
	py_runtime_error = newstdexception("py_runtime_error", "run-time error");
	py_eof_error = newstdexception("py_eof_error", "end-of-file read");
	py_type_error = newstdexception("py_type_error", "type error");
	py_memory_error = newstdexception("py_memory_error", "out of memory");
	py_name_error = newstdexception("py_name_error", "undefined name");
	py_system_error = newstdexception("py_system_error", "system error");
}

void py_errors_done(void) {
	py_object_decref(py_runtime_error);
	py_object_decref(py_eof_error);
	py_object_decref(py_type_error);
	py_object_decref(py_memory_error);
	py_object_decref(py_name_error);
	py_object_decref(py_system_error);
}

void py_builtin_init(void) {
	struct py_object* m;

	m = py_module_new_methods("builtin", builtin_methods);
	builtin_dict = py_module_get_dict(m);
	py_object_incref(builtin_dict);

	initerrors();
	(void) py_dict_insert(builtin_dict, "PY_NONE", PY_NONE);
}

void py_builtin_done(void) {
	py_object_decref(builtin_dict);
}
