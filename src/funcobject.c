/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/funcobject.h>

typedef struct {
	struct py_object ob;
	struct py_object* func_code;
	struct py_object* func_globals;
} funcobject;

struct py_object* py_func_new(
		struct py_object* code, struct py_object* globals) {

	funcobject* op = py_object_new(&py_func_type);
	if(op != NULL) {
		py_object_incref(code);
		op->func_code = code;
		py_object_incref(globals);
		op->func_globals = globals;
	}

	return (struct py_object*) op;
}

struct py_object* py_func_get_code(struct py_object* op) {
	if(!py_is_func(op)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((funcobject*) op)->func_code;
}

struct py_object* py_func_get_globals(struct py_object* op) {
	if(!py_is_func(op)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((funcobject*) op)->func_globals;
}

static void func_dealloc(struct py_object* op) {
	funcobject* fp = (funcobject*) op;

	py_object_decref(fp->func_code);
	py_object_decref(fp->func_globals);
	free(op);
}

struct py_type py_func_type = {
		{ 1, 0, &py_type_type }, "function", sizeof(funcobject),
		func_dealloc, /* dealloc */
		0, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0 };
