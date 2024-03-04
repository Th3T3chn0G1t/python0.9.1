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

struct py_object* py_module_new(const char* name) {
	struct py_module* m = py_object_new(&py_module_type);
	if(m == NULL) return NULL;

	m->name = py_string_new(name);
	m->attr = py_dict_new();
	if(m->name == NULL || m->attr == NULL) {
		py_object_decref(m);
		return NULL;
	}

	return (struct py_object*) m;
}

struct py_object* py_module_get_dict(m)struct py_object* m;
{
	if(!py_is_module(m)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((struct py_module*) m)->attr;
}

const char* py_module_get_name(struct py_object* m) {
	if(!py_is_module(m)) {
		py_error_set_badarg();
		return NULL;
	}

	return py_string_get(((struct py_module*) m)->name);
}

/* Methods */

static void module_dealloc(m)struct py_module* m;
{
	if(m->name != NULL)
		py_object_decref(m->name);
	if(m->attr != NULL)
		py_object_decref(m->attr);
}

static struct py_object* module_getattr(m, name)struct py_module* m;
												char* name;
{
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
	if(res == NULL) {
		py_error_set_string(py_name_error, name);
	}
	else
		py_object_incref(res);
	return res;
}

struct py_type py_module_type = {
		{ 1, 0, &py_type_type }, /* size */
		sizeof(struct py_module), /* tp_size */
		module_dealloc, /* dealloc */
		module_getattr, /* get_attr */
		0, /* cmp */
		0 };
