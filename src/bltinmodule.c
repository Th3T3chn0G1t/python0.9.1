/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Built-in functions */

#include <python/state.h>
#include <python/node.h>
#include <python/import.h>
#include <python/modsupport.h>
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

static struct py_object* py_builtin_float(
	struct py_env* env, struct py_object* self, struct py_object* args) {

	(void) env;
	(void) self;

	if(args && args->type == PY_TYPE_FLOAT) {
		py_object_incref(args);
		return args;
	}
	else if(args && args->type == PY_TYPE_INT) {
		py_value_t x = py_int_get(args);
		return py_float_new((double) x);
	}

	py_error_set_string(
			py_type_error, "float() argument must be float or int");
	return NULL;
}

static struct py_object* py_builtin_int(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	(void) env;
	(void) self;

	if(args && args->type == PY_TYPE_INT) {
		py_object_incref(args);
		return args;
	}
	else if(args && args->type == PY_TYPE_FLOAT) {
		double x = py_float_get(args);
		return py_int_new((py_value_t) x);
	}

	py_error_set_string(py_type_error, "int() argument must be float or int");
	return NULL;
}

static struct py_object* py_builtin_len(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	py_value_t len;

	(void) env;
	(void) self;

	if(args == NULL) {
		py_error_set_string(py_type_error, "len() without argument");
		return NULL;
	}

	if(py_is_varobject(args)) len = py_varobject_size(args);
	else if(args->type == PY_TYPE_DICT) len = ((struct py_dict*) args)->used;
	else {
		py_error_set_string(py_type_error, "len() of unsized object");
		return NULL;
	}

	return py_int_new(len);
}

/*
 * TODO: This can probably be simplified/split-off since it's such a core
 * 		 Function.
 */
static struct py_object* py_builtin_range(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	static const char errmsg[] = "range() requires 1-3 int arguments";

	unsigned i, n;
	py_value_t ilow, ihigh, istep;

	(void) env;
	(void) self;

	if(args && (args->type == PY_TYPE_INT)) {
		ilow = 0;
		ihigh = py_int_get(args);
		istep = 1;
	}
	else if(!args || args->type != PY_TYPE_TUPLE) {
		py_error_set_string(py_type_error, errmsg);
		return NULL;
	}
	else {
		n = py_varobject_size(args);

		if(n < 1 || n > 3) {
			py_error_set_string(py_type_error, errmsg);
			return NULL;
		}

		for(i = 0; i < n; i++) {
			if(py_tuple_get(args, i)->type != PY_TYPE_INT) {
				py_error_set_string(py_type_error, errmsg);
				return NULL;
			}
		}

		if(n == 3) {
			istep = py_int_get(py_tuple_get(args, 2));
			--n;
		}
		else istep = 1;

		ihigh = py_int_get(py_tuple_get(args, --n));

		if(n > 0) ilow = py_int_get(py_tuple_get(args, 0));
		else ilow = 0;
	}

	if(istep == 0) {
		py_error_set_string(py_runtime_error, "zero step for range()");
		return NULL;
	}

	/* TODO: ought to check overflow of subion */
	if(istep > 0) n = (unsigned) ((ihigh - ilow + istep - 1) / istep);
	else n = (unsigned) ((ihigh - ilow + istep + 1) / istep);

	if(!(args = py_list_new(n))) return 0;

	for(i = 0; i < n; i++) {
		struct py_object* w = py_int_new(ilow);

		if(w == NULL) {
			py_object_decref(args);
			return NULL;
		}

		py_list_set(args, i, w);
		ilow += istep;
	}

	return args;
}

static struct py_object* py_builtin_append(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	struct py_object* lp = py_tuple_get(args, 0);
	struct py_object* op = py_tuple_get(args, 1);

	(void) env;
	(void) self;

	if(!lp || !op || py_list_add(lp, op) == -1) return 0;

	return py_object_incref(PY_NONE);
}

static struct py_object* py_builtin_insert(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	struct py_object* lp;
	struct py_object* ind;
	struct py_object* op;

	(void) env;
	(void) self;

	if(!args || !(args->type == PY_TYPE_TUPLE) || py_varobject_size(args) != 2 ||
			!(lp = py_tuple_get(args, 0)) || !(lp->type == PY_TYPE_LIST) ||
			!(ind = py_tuple_get(args, 1)) || !(ind->type == PY_TYPE_INT) ||
			!(op = py_tuple_get(args, 2))) {

		py_error_set_badarg();
		return NULL;
	}

	if(py_list_insert(lp, (unsigned) py_int_get(ind), op) == -1) return NULL;

	return py_object_incref(PY_NONE);
}

static struct py_object* py_builtin_pass(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	(void) env;
	(void) self;
	(void) args;

	return py_object_incref(PY_NONE);
}

static struct py_object* py_builtin_notv(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	struct py_object* retval;
	py_value_t val;

	(void) env;
	(void) self;

	if(args->type != PY_TYPE_INT) {
		py_error_set_badarg();
		return 0;
	}

	val = py_int_get(args);
	if(py_error_occurred()) return 0;

	if(val) retval = PY_FALSE;
	else retval = PY_TRUE;

	return py_object_incref(retval);
}

/* TODO: Python global state. */
static struct py_methodlist py_builtin_methods[] = {
		{ "float", py_builtin_float },
		{ "int", py_builtin_int },
		{ "len", py_builtin_len },
		{ "range", py_builtin_range },
		{ "append", py_builtin_append },
		{ "insert", py_builtin_insert },
		{ "pass", py_builtin_pass },
		{ "notv", py_builtin_notv },
		{ NULL, NULL } };

/* TODO: Python global state. */
static struct py_object* py_builtin_dict;

struct py_object* py_builtin_get(const char* name) {
	return py_dict_lookup(py_builtin_dict, name);
}

static struct py_object* newstdexception(char* name, char* message) {
	struct py_object* v = py_string_new(message);

	if(v == NULL || py_dict_insert(py_builtin_dict, name, v) != 0) {
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

	m = py_module_new_methods("builtin", py_builtin_methods);
	py_builtin_dict = ((struct py_module*) m)->attr;
	py_object_incref(py_builtin_dict);

	if(py_dict_insert(py_builtin_dict, "true", PY_TRUE) == -1) {
		py_fatal("could not add `true' object");
	}

	if(py_dict_insert(py_builtin_dict, "false", PY_FALSE) == -1) {
		py_fatal("could not add `false' object");
	}

	initerrors();
	(void) py_dict_insert(py_builtin_dict, "PY_NONE", PY_NONE);
}

void py_builtin_done(void) {
	py_object_decref(py_builtin_dict);
}
