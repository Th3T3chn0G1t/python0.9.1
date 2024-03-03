/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Class object implementation */

#include <python/std.h>
#include <python/structmember.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/classobject.h>
#include <python/dictobject.h>
#include <python/tupleobject.h>
#include <python/funcobject.h>

struct py_class {
	struct py_object ob;
	struct py_object* bases; /* A tuple */
	struct py_object* methods; /* A dictionary */
};
struct py_object* py_class_new(
		struct py_object* bases, struct py_object* methods) {

	struct py_class* op;

	op = py_object_new(&py_class_type);
	if(op == NULL) return NULL;

	if(bases != NULL) py_object_incref(bases);

	op->bases = bases;
	py_object_incref(methods);
	op->methods = methods;

	return (struct py_object*) op;
}

/* Class methods */

static void py_class_dealloc(struct py_object* op) {
	struct py_class* cls = (struct py_class*) op;

	py_object_decref(cls->bases);
	py_object_decref(cls->methods);
}

static struct py_object* py_class_get_attr(
		struct py_object* op, const char* name) {

	struct py_object* v;
	struct py_class* cls;

	cls = (struct py_class*) op;
	v = py_dict_lookup(cls->methods, name);

	if(v != NULL) {
		py_object_incref(v);
		return v;
	}

	if(cls->bases != NULL) {
		int n = py_varobject_size(cls->bases);
		int i;

		for(i = 0; i < n; i++) {
			v = py_class_get_attr(py_tuple_get(cls->bases, i), name);
			if(v != NULL) return v;

			py_error_clear();
		}
	}

	py_error_set_string(py_name_error, name);
	return NULL;
}

struct py_type py_class_type = {
		{ 1, 0, &py_type_type }, "class", sizeof(struct py_class),
		py_class_dealloc, /* dealloc */
		py_class_get_attr, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, /* sequencemethods */
};

/* We're not done yet: next, we define class member objects... */
typedef struct {
	struct py_object ob;
	struct py_class* cm_class;      /* The class object */
	struct py_object* cm_attr;       /* A dictionary */
} classmemberobject;

struct py_object* py_classmember_new(class)struct py_object* class;
{
	classmemberobject* cm;
	if(!py_is_class(class)) {
		py_error_set_badcall();
		return NULL;
	}
	cm = py_object_new(&py_class_member_type);
	if(cm == NULL) {
		return NULL;
	}
	py_object_incref(class);
	cm->cm_class = (struct py_class*) class;
	cm->cm_attr = py_dict_new();
	if(cm->cm_attr == NULL) {
		py_object_decref(cm);
		return NULL;
	}
	return (struct py_object*) cm;
}

/* Class member methods */

static void classmember_dealloc(struct py_object* op) {
	classmemberobject* cm = (classmemberobject*) op;

	py_object_decref(cm->cm_class);
	py_object_decref(cm->cm_attr);
}

static struct py_object* classmember_getattr(
		struct py_object* op, const char* name) {

	classmemberobject* cm = (classmemberobject*) op;
	struct py_object* v = py_dict_lookup(cm->cm_attr, name);

	if(v != NULL) {
		py_object_incref(v);
		return v;
	}

	v = py_class_get_attr((struct py_object*) cm->cm_class, name);
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

static int classmember_setattr(cm, name, v)classmemberobject* cm;
										   char* name;
										   struct py_object* v;
{
	if(v == NULL) {
		return py_dict_remove(cm->cm_attr, name);
	}
	else {
		return py_dict_insert(cm->cm_attr, name, v);
	}
}

struct py_type py_class_member_type = {
		{ 1, 0, &py_type_type }, "class member",
		sizeof(classmemberobject), classmember_dealloc, /* dealloc */
		classmember_getattr, /* get_attr */
		classmember_setattr, /* set_attr */
		0, /* cmp */
		0, /* sequencemethods */
};


/* And finally, here are class method objects */

/* (Really methods of class members) */

typedef struct {
	struct py_object ob;
	struct py_object* cm_func;       /* The method function */
	struct py_object* cm_self;       /* The object to which this applies */
} classmethodobject;

struct py_object* py_classmethod_new(func, self)struct py_object* func;
												struct py_object* self;
{
	classmethodobject* cm;
	if(!py_is_func(func)) {
		py_error_set_badcall();
		return NULL;
	}
	cm = py_object_new(&py_class_method_type);
	if(cm == NULL) {
		return NULL;
	}
	py_object_incref(func);
	cm->cm_func = func;
	py_object_incref(self);
	cm->cm_self = self;
	return (struct py_object*) cm;
}

struct py_object* py_classmethod_get_func(cm)struct py_object* cm;
{
	if(!py_is_classmethod(cm)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((classmethodobject*) cm)->cm_func;
}

struct py_object* py_classmethod_get_self(cm)struct py_object* cm;
{
	if(!py_is_classmethod(cm)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((classmethodobject*) cm)->cm_self;
}

/* Class method methods */

#define OFF(x) offsetof(classmethodobject, x)

static struct py_structmember classmethod_memberlist[] = {
		{ "cm_func", PY_TYPE_OBJECT, OFF(cm_func), PY_READWRITE },
		{ "cm_self", PY_TYPE_OBJECT, OFF(cm_self), PY_READWRITE },
		{ NULL,      0, 0,                         0 }  /* Sentinel */
};

static struct py_object* classmethod_getattr(cm, name)classmethodobject* cm;
													  char* name;
{
	return py_struct_get(cm, classmethod_memberlist, name);
}

static void classmethod_dealloc(struct py_object* op) {
	classmethodobject* cm = (classmethodobject*) op;

	py_object_decref(cm->cm_func);
	py_object_decref(cm->cm_self);
}

struct py_type py_class_method_type = {
		{ 1, 0, &py_type_type }, "class method",
		sizeof(classmethodobject), classmethod_dealloc, /* dealloc */
		classmethod_getattr, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, /* sequencemethods */
};
