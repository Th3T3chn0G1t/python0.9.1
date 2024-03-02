/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object implementation */

#include <python/std.h>
#include <python/structmember.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/funcobject.h>

typedef struct {
	struct py_object ob;
	struct py_object* func_code;
	struct py_object* func_globals;
} funcobject;

struct py_object* py_func_new(code, globals)struct py_object* code;
											struct py_object* globals;
{
	funcobject* op = py_object_new(&py_func_type);
	if(op != NULL) {
		py_object_incref(code);
		op->func_code = code;
		py_object_incref(globals);
		op->func_globals = globals;
	}
	return (struct py_object*) op;
}

struct py_object* py_func_get_code(op)struct py_object* op;
{
	if(!py_is_func(op)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((funcobject*) op)->func_code;
}

struct py_object* py_func_get_globals(op)struct py_object* op;
{
	if(!py_is_func(op)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((funcobject*) op)->func_globals;
}

/* Methods */

#define OFF(x) offsetof(funcobject, x)

static struct py_structmember func_memberlist[] = {
		{ "func_code",    PY_TYPE_OBJECT, OFF(func_code),    PY_READWRITE },
		{ "func_globals", PY_TYPE_OBJECT, OFF(func_globals), PY_READWRITE },
		{ NULL,           0, 0,                              0 }  /* Sentinel */
};

static struct py_object* func_getattr(op, name)funcobject* op;
											   char* name;
{
	return py_struct_get(op, func_memberlist, name);
}

static void func_dealloc(op)funcobject* op;
{
	py_object_decref(op->func_code);
	py_object_decref(op->func_globals);
	free(op);
}

struct py_type py_func_type = {
		{ 1, &py_type_type, 0 }, "function", sizeof(funcobject),
		func_dealloc, /* dealloc */
		func_getattr, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, 0, 0 };
