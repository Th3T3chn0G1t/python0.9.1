/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Class object implementation */

#include <python/std.h>

#include <python/object.h>
#include <python/object/class.h>
#include <python/object/dict.h>

struct py_object* py_class_new(struct py_object* methods) {
	struct py_class* op;

	if(!(op = py_object_new(PY_TYPE_CLASS))) return 0;

	op->attr = py_object_incref(methods);

	return (void*) op;
}

/* Class methods */

void py_class_dealloc(struct py_object* op) {
	py_object_decref(((struct py_class*) op)->attr);
}

struct py_object* py_class_get_attr(struct py_object* op, const char* name) {
	struct py_object* v;

	if((v = py_dict_lookup(((struct py_class*) op)->attr, name))) {
		return py_object_incref(v);
	}

	return 0;
}

/* We're not done yet: next, we define class member objects... */

struct py_object* py_class_member_new(struct py_object* class) {
	struct py_class_member* cm;

	if(!(cm = py_object_new(PY_TYPE_CLASS_MEMBER))) return 0;

	cm->class = py_object_incref(class);

	if(!(cm->attr = py_dict_new())) {
		py_object_decref(cm);
		return 0;
	}

	return (void*) cm;
}

/* Class member methods */

void py_class_member_dealloc(struct py_object* op) {
	struct py_class_member* cm = (void*) op;

	py_object_decref(cm->class);
	py_object_decref(cm->attr);
}

struct py_object* py_class_member_get_attr(
		struct py_object* op, const char* name) {

	struct py_class_member* cm = (void*) op;
	struct py_object* v;

	if((v = py_dict_lookup(cm->attr, name))) return py_object_incref(v);

	if(!(v = py_class_get_attr((void*) cm->class, name))) return v;

	if(v->type == PY_TYPE_FUNC) {
		struct py_object* w = py_class_method_new(v, (struct py_object*) cm);
		py_object_decref(v);
		return w;
	}

	py_object_decref(v);
	return 0;
}

/* And finally, here are class method objects */
/* (Really methods of class members) */

struct py_object* py_class_method_new(
		struct py_object* func, struct py_object* self) {

	struct py_class_method* cm;

	if(!(cm = py_object_new(PY_TYPE_CLASS_METHOD))) return 0;

	cm->func =py_object_incref(func);
	cm->self = py_object_incref(self);

	return (void*) cm;
}

struct py_object* py_class_method_get_func(struct py_object* cm) {
	return ((struct py_class_method*) cm)->func;
}

struct py_object* py_class_method_get_self(struct py_object* cm) {
	return ((struct py_class_method*) cm)->self;
}

/* Class method methods */

void py_class_method_dealloc(struct py_object* op) {
	struct py_class_method* cm = (void*) op;

	py_object_decref(cm->func);
	py_object_decref(cm->self);
}
