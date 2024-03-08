/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Class object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/classobject.h>
#include <python/dictobject.h>
#include <python/tupleobject.h>
#include <python/funcobject.h>

struct py_object* py_class_new(struct py_object* methods) {

	struct py_class* op;

	op = py_object_new(&py_class_type);
	if(op == NULL) return NULL;

	py_object_incref(methods);
	op->attr = methods;

	return (struct py_object*) op;
}

/* Class methods */

static void py_class_dealloc(struct py_object* op) {
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

struct py_type py_class_type = {
		{ &py_type_type, 1 }, sizeof(struct py_class),
		py_class_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};

/* We're not done yet: next, we define class member objects... */

struct py_object* py_classmember_new(struct py_object* class) {
	struct py_classmember* cm;

	if(!py_is_class(class)) {
		py_error_set_badcall();
		return NULL;
	}

	cm = py_object_new(&py_class_member_type);
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

static void py_classmember_dealloc(struct py_object* op) {
	struct py_classmember* cm = (struct py_classmember*) op;

	py_object_decref(cm->class);
	py_object_decref(cm->attr);
}

struct py_object* py_classmember_get_attr(
		struct py_object* op, const char* name) {

	struct py_classmember* cm = (struct py_classmember*) op;
	struct py_object* v = py_dict_lookup(cm->attr, name);

	if(v != NULL) {
		py_object_incref(v);
		return v;
	}

	v = py_class_get_attr((struct py_object*) cm->class, name);
	if(v == NULL) return v;

	if(py_is_func(v)) {
		struct py_object* w = py_classmethod_new(v, (struct py_object*) cm);
		py_object_decref(v);
		return w;
	}

	py_object_decref(v);
	py_error_set_string(py_name_error, name);
	return NULL;
}

struct py_type py_class_member_type = {
		{ &py_type_type, 1 },
		sizeof(struct py_classmember), py_classmember_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};


/* And finally, here are class method objects */
/* (Really methods of class members) */

struct py_object* py_classmethod_new(
		struct py_object* func, struct py_object* self) {

	struct py_classmethod* cm;

	if(!py_is_func(func)) {
		py_error_set_badcall();
		return NULL;
	}

	cm = py_object_new(&py_class_method_type);
	if(cm == NULL) return NULL;

	py_object_incref(func);
	cm->func = func;
	py_object_incref(self);
	cm->self = self;

	return (struct py_object*) cm;
}

struct py_object* py_classmethod_get_func(struct py_object* cm) {
	if(!py_is_classmethod(cm)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_classmethod*) cm)->func;
}

struct py_object* py_classmethod_get_self(struct py_object* cm) {
	if(!py_is_classmethod(cm)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_classmethod*) cm)->self;
}

/* Class method methods */

static void py_classmethod_dealloc(struct py_object* op) {
	struct py_classmethod* cm = (struct py_classmethod*) op;

	py_object_decref(cm->func);
	py_object_decref(cm->self);
}

struct py_type py_class_method_type = {
		{ &py_type_type, 1 },
		sizeof(struct py_classmethod), py_classmethod_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};
