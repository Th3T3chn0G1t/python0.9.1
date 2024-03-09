/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/dictobject.h>
#include <python/stringobject.h>
#include <python/moduleobject.h>

int py_is_module(const void* op) {
	return ((struct py_object*) op)->type == &py_module_type;
}

struct py_object* py_module_new(const char* name) {
	struct py_module* m = py_object_new(&py_module_type);
	if(m == NULL) return NULL;

	m->name = py_string_new(name);
	if(m->name == NULL) {
		py_object_decref(m);
		return NULL;
	}

	m->attr = py_dict_new();
	if(m->attr == NULL) {
		py_object_decref(m->name);
		py_object_decref(m);
		return NULL;
	}

	return (struct py_object*) m;
}

/* Methods */

static void py_module_dealloc(struct py_object* op) {
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

struct py_type py_module_type = {
		{ &py_type_type, 1 }, /* size */
		sizeof(struct py_module), /* tp_size */
		py_module_dealloc, /* dealloc */
		0, /* cmp */
		0 };
