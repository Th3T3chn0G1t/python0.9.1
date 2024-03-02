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

typedef struct {
	struct py_object ob;
	struct py_object* md_name;
	struct py_object* md_dict;
} moduleobject;

struct py_object* py_module_new(name)char* name;
{
	moduleobject* m = py_object_new(&py_module_type);
	if(m == NULL) {
		return NULL;
	}
	m->md_name = py_string_new(name);
	m->md_dict = py_dict_new();
	if(m->md_name == NULL || m->md_dict == NULL) {
		PY_DECREF(m);
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
	return ((moduleobject*) m)->md_dict;
}

char* py_module_get_name(m)struct py_object* m;
{
	if(!py_is_module(m)) {
		py_error_set_badarg();
		return NULL;
	}
	return py_string_get_value(((moduleobject*) m)->md_name);
}

/* Methods */

static void module_dealloc(m)moduleobject* m;
{
	if(m->md_name != NULL)
		PY_DECREF(m->md_name);
	if(m->md_dict != NULL)
		PY_DECREF(m->md_dict);
	free(m);
}

static void module_print(m, fp, flags)moduleobject* m;
									  FILE* fp;
									  int flags;
{
	(void) flags;
	fprintf(fp, "<module '%s'>", py_string_get_value(m->md_name));
}

static struct py_object* module_repr(m)moduleobject* m;
{
	char buf[100];
	sprintf(buf, "<module '%.80s'>", py_string_get_value(m->md_name));
	return py_string_new(buf);
}

static struct py_object* module_getattr(m, name)moduleobject* m;
												char* name;
{
	struct py_object* res;
	if(strcmp(name, "__dict__") == 0) {
		PY_INCREF(m->md_dict);
		return m->md_dict;
	}
	if(strcmp(name, "__name__") == 0) {
		PY_INCREF(m->md_name);
		return m->md_name;
	}
	res = py_dict_lookup(m->md_dict, name);
	if(res == NULL) {
		py_error_set_string(py_name_error, name);
	}
	else
		PY_INCREF(res);
	return res;
}

static int module_setattr(m, name, v)moduleobject* m;
									 char* name;
									 struct py_object* v;
{
	if(strcmp(name, "__dict__") == 0 || strcmp(name, "__name__") == 0) {
		py_error_set_string(
				py_name_error, "can't assign to reserved member name");
		return -1;
	}
	if(v == NULL) {
		return py_dict_remove(m->md_dict, name);
	}
	else {
		return py_dict_insert(m->md_dict, name, v);
	}
}

struct py_type py_module_type = {
		{ 1, &py_type_type, 0 }, /* size */
		"module", /* name */
		sizeof(moduleobject), /* tp_size */
		0, /* itemsize */
		module_dealloc, /* dealloc */
		module_print, /* print */
		module_getattr, /* get_attr */
		module_setattr, /* set_attr */
		0, /* cmp */
		module_repr, /* repr */
		0, 0, 0 };
