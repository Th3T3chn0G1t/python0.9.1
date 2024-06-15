/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/object/dict.h>
#include <python/object/string.h>
#include <python/object/module.h>

struct py_object* py_module_new(const char* name) {
	struct py_module* m;

	if(!(m = py_object_new(PY_TYPE_MODULE))) return 0;

	if(!(m->name = py_string_new(name))) {
		py_object_decref(m);
		return NULL;
	}

	if(!(m->attr = py_dict_new())) {
		py_object_decref(m->name);
		py_object_decref(m);
		return 0;
	}

	return (void*) m;
}

/* Methods */

void py_module_dealloc(struct py_object* op) {
	struct py_module* m = (struct py_module*) op;
	py_object_decref(m->name);
	py_object_decref(m->attr);
}

struct py_object* py_module_get_attr(struct py_object* op, const char* name) {
	struct py_module* m = (struct py_module*) op;
	struct py_object* res;

	if(strcmp(name, "__dict__") == 0) {
		py_object_incref(m->attr);
		return m->attr;
	}

	if(strcmp(name, "__name__") == 0) {
		py_object_incref(m->name);
		return m->name;
	}

	res = py_dict_lookup(m->attr, name);
	if(res == NULL) py_error_set_string(py_name_error, name);
	else py_object_incref(res);

	return res;
}
