/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module object implementation */

#include <python/std.h>
#include <python/import.h>
#include <python/state.h>

#include <python/object.h>
#include <python/object/dict.h>
#include <python/object/string.h>
#include <python/object/module.h>
#include <python/object/method.h>

struct py_object* py_module_new(const char* name) {
	struct py_module* m;

	if(!(m = py_object_new(PY_TYPE_MODULE))) return 0;

	if(!(m->name = py_string_new(name))) {
		py_object_decref(m);
		return 0;
	}

	if(!(m->attr = py_dict_new())) {
		py_object_decref(m->name);
		py_object_decref(m);
		return 0;
	}

	return (void*) m;
}


/* TODO: Better EH. */
struct py_object* py_module_new_methods(
		struct py_env* env, const char* name,
		const struct py_methodlist* methods) {

	struct py_object* m;
	struct py_object* d;
	struct py_object* v;

	if(!(m = py_module_add(env, name))) return 0;

	d = ((struct py_module*) m)->attr;

	for(; methods->name; methods++) {
		v = py_method_new(methods->method, (struct py_object*) NULL);

		if(!v || py_dict_insert(d, methods->name, v) == -1) return 0;

		py_object_decref(v);
	}

	return m;
}

/* Methods */

void py_module_dealloc(struct py_object* op) {
	struct py_module* m = (void*) op;

	py_object_decref(m->name);
	py_object_decref(m->attr);

	free(op);
}

struct py_object* py_module_get_attr(struct py_object* op, const char* name) {
	struct py_module* m = (void*) op;

	/* TODO: Remove. */
	if(!strcmp(name, "__dict__")) return py_object_incref(m->attr);
	if(!strcmp(name, "__name__")) return py_object_incref(m->name);

	return py_object_incref(py_dict_lookup(m->attr, name));
}
