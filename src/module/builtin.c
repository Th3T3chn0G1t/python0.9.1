/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Built-in functions */

#include <python/state.h>
#include <python/node.h>
#include <python/import.h>
#include <python/errors.h>

#include <python/module/builtin.h>

#include <python/object/int.h>
#include <python/object/float.h>
#include <python/object/string.h>
#include <python/object/module.h>
#include <python/object/list.h>
#include <python/object/dict.h>
#include <python/object/tuple.h>

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
	py_value_t low, high, step;

	(void) env;
	(void) self;

	if(args && (args->type == PY_TYPE_INT)) {
		low = 0;
		high = py_int_get(args);
		step = 1;
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
			step = py_int_get(py_tuple_get(args, 2));
			--n;
		}
		else step = 1;

		high = py_int_get(py_tuple_get(args, --n));

		if(n > 0) low = py_int_get(py_tuple_get(args, 0));
		else low = 0;
	}

	if(step == 0) {
		py_error_set_string(py_runtime_error, "zero step for range()");
		return NULL;
	}

	/* TODO: ought to check overflow of subion */
	if(step > 0) n = (unsigned) ((high - low + step - 1) / step);
	else n = (unsigned) ((high - low + step + 1) / step);

	if(!(args = py_list_new(n))) return py_error_set_nomem();

	for(i = 0; i < n; i++) {
		struct py_object* w = py_int_new(low);

		if(w == NULL) {
			py_object_decref(args);
			return NULL;
		}

		py_list_set(args, i, w);
		low += step;
	}

	return args;
}

static struct py_object* py_builtin_append(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	struct py_object* lp;
	struct py_object* op;

	(void) env;
	(void) self;

	if(!args || args->type != PY_TYPE_TUPLE ||
		(lp = py_tuple_get(args, 0))->type != PY_TYPE_LIST) {

		py_error_set_badarg();
		return 0;
	}

	op = py_tuple_get(args, 1);

	if(py_list_add(lp, op) == -1) return py_error_set_nomem();

	return py_object_incref(PY_NONE);
}

static struct py_object* py_builtin_insert(
		struct py_env* env, struct py_object* self, struct py_object* args) {

	struct py_object* lp;
	struct py_object* ind;
	struct py_object* op;

	(void) env;
	(void) self;

	if(!args || args->type != PY_TYPE_TUPLE || py_varobject_size(args) != 2 ||
			!(lp = py_tuple_get(args, 0)) || lp->type != PY_TYPE_LIST ||
			!(ind = py_tuple_get(args, 1)) || ind->type != PY_TYPE_INT ||
			!(op = py_tuple_get(args, 2))) {

		py_error_set_badarg();
		return 0;
	}

	if(py_list_insert(lp, (unsigned) py_int_get(ind), op) == -1) {
		py_error_set_nomem();
		return 0;
	}

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

static struct py_object* py_exception_new(
		const char* name, const char* message) {

	struct py_object* v;

	if(!(v = py_string_new(message))) return 0;

	if(py_dict_insert(py_builtin_dict, name, v) == -1) return 0;

	return v;
}

static enum py_result py_init_exceptions(void) {
	if(!(py_runtime_error = py_exception_new(
			"py_runtime_error", "run-time error"))) {

		return PY_RESULT_OOM;
	}
	if(!(py_eof_error = py_exception_new(
			"py_eof_error", "end-of-file read"))) {

		return PY_RESULT_OOM;
	}
	if(!(py_type_error = py_exception_new(
			"py_type_error", "type error"))) {

		return PY_RESULT_OOM;
	}
	if(!(py_memory_error = py_exception_new(
			"py_memory_error", "out of memory"))) {

		return PY_RESULT_OOM;
	}
	if(!(py_name_error = py_exception_new(
			"py_name_error", "undefined name"))) {

		return PY_RESULT_OOM;
	}
	if(!(py_system_error = py_exception_new(
			"py_system_error", "system error"))) {

		return PY_RESULT_OOM;
	}

	return PY_RESULT_OK;
}

void py_errors_done(void) {
	py_object_decref(py_runtime_error);
	py_object_decref(py_eof_error);
	py_object_decref(py_type_error);
	py_object_decref(py_memory_error);
	py_object_decref(py_name_error);
	py_object_decref(py_system_error);
}

enum py_result py_builtin_init(struct py_env* env) {
	struct py_object* m;

	if(!(m = py_module_new_methods(env, "builtin", py_builtin_methods))) {
		return PY_RESULT_OOM;
	}

	py_builtin_dict = py_object_incref(((struct py_module*) m)->attr);

	if(py_dict_insert(py_builtin_dict, "true", PY_TRUE) == -1) {
		return PY_RESULT_ERROR;
	}

	if(py_dict_insert(py_builtin_dict, "false", PY_FALSE) == -1) {
		return PY_RESULT_ERROR;
	}

	if(py_dict_insert(py_builtin_dict, "none", PY_NONE) == -1) {
		return PY_RESULT_ERROR;
	}

	return py_init_exceptions();
}

void py_builtin_done(void) {
	py_object_decref(py_builtin_dict);
}
