/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Class object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/object/class.h>
#include <python/object/dict.h>
#include <python/object/tuple.h>
#include <python/object/func.h>

struct py_object* py_class_new(struct py_object* methods) {
	struct py_class* op;

	op = py_object_new(PY_TYPE_CLASS);
	if(op == NULL) return NULL;

	py_object_incref(methods);
	op->attr = methods;

	return (struct py_object*) op;
}

/* Class methods */

void py_class_dealloc(struct py_object* op) {
	py_object_decref(((struct py_class*) op)->attr);
}

struct py_object* py_class_get_attr(struct py_object* op, const char* name) {
	struct py_object* v = py_dict_lookup(((struct py_class*) op)->attr, name);

	if(v != NULL) {
		py_object_incref(v);
		return v;
	}

	py_error_set_string(py_name_error, name);
	return NULL;
}

/* We're not done yet: next, we define class member objects... */

struct py_object* py_class_member_new(struct py_object* class) {
	struct py_class_member* cm;

	if(!(class->type == PY_TYPE_CLASS)) {
		py_error_set_badcall();
		return NULL;
	}

	cm = py_object_new(PY_TYPE_CLASS_MEMBER);
	if(cm == NULL) return NULL;

	py_object_incref(class);
	cm->class = (struct py_class*) class;
	cm->attr = py_dict_new();

	if(cm->attr == NULL) {
		py_object_decref(cm);
		return NULL;
	}

	return (struct py_object*) cm;
}

/* Class member methods */

void py_class_member_dealloc(struct py_object* op) {
	struct py_class_member* cm = (struct py_class_member*) op;

	py_object_decref(cm->class);
	py_object_decref(cm->attr);
}

struct py_object* py_class_member_get_attr(
		struct py_object* op, const char* name) {

	struct py_class_member* cm = (struct py_class_member*) op;
	struct py_object* v = py_dict_lookup(cm->attr, name);

	if(v != NULL) {
		py_object_incref(v);
		return v;
	}

	v = py_class_get_attr((struct py_object*) cm->class, name);
	if(v == NULL) return v;

	if(v->type == PY_TYPE_FUNC) {
		struct py_object* w = py_class_method_new(v, (struct py_object*) cm);
		py_object_decref(v);
		return w;
	}

	py_object_decref(v);
	py_error_set_string(py_name_error, name);
	return NULL;
}

/* And finally, here are class method objects */
/* (Really methods of class members) */

struct py_object* py_class_method_new(
		struct py_object* func, struct py_object* self) {

	struct py_class_method* cm;

	if(!(func->type == PY_TYPE_FUNC)) {
		py_error_set_badcall();
		return NULL;
	}

	cm = py_object_new(PY_TYPE_CLASS_METHOD);
	if(cm == NULL) return NULL;

	py_object_incref(func);
	cm->func = func;
	py_object_incref(self);
	cm->self = self;

	return (struct py_object*) cm;
}

struct py_object* py_class_method_get_func(struct py_object* cm) {
	if(!(cm->type == PY_TYPE_CLASS_METHOD)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_class_method*) cm)->func;
}

struct py_object* py_class_method_get_self(struct py_object* cm) {
	if(!(cm->type == PY_TYPE_CLASS_METHOD)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_class_method*) cm)->self;
}

/* Class method methods */

void py_class_method_dealloc(struct py_object* op) {
	struct py_class_method* cm = (struct py_class_method*) op;

	py_object_decref(cm->func);
	py_object_decref(cm->self);
}
