/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Class object interface */

#ifndef PY_CLASSOBJECT_H
#define PY_CLASSOBJECT_H

#include <python/object.h>

/*
 * Classes are really hacked in at the last moment.
 * It should be possible to use other object types as base classes,
 * but currently it isn't. We'll see if we can fix that later, sigh...
 */

struct py_class {
	struct py_object ob;
	struct py_object* attr; /* A dictionary */
};

struct py_class_member {
	struct py_object ob;
	struct py_class* class; /* The class object */
	struct py_object* attr; /* A dictionary */
};

struct py_class_method {
	struct py_object ob;
	struct py_object* func; /* The method function */
	struct py_object* self; /* The object to which this applies */
};

/* TODO: Python global state. */
extern struct py_type py_class_type;
extern struct py_type py_class_member_type;
extern struct py_type py_class_method_type;

int py_is_class(const void*);
int py_is_class_member(const void*);
int py_is_class_method(const void*);

struct py_object* py_class_new(struct py_object*);
struct py_object* py_class_get_attr(struct py_object*, const char*);

struct py_object* py_class_member_new(struct py_object*);
struct py_object* py_class_member_get_attr(struct py_object*, const char*);

struct py_object* py_class_method_new(struct py_object*, struct py_object*);
struct py_object* py_class_method_get_func(struct py_object*);
struct py_object* py_class_method_get_self(struct py_object*);

#endif
