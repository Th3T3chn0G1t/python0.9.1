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

struct py_classmember {
	struct py_object ob;
	struct py_class* class; /* The class object */
	struct py_object* attr; /* A dictionary */
};

/* TODO: Python global state. */
extern struct py_type py_class_type;
extern struct py_type py_class_member_type;
extern struct py_type py_class_method_type;

#define py_is_class(op) ((op)->type == &py_class_type)
#define py_is_classmember(op) ((op)->type == &py_class_member_type)
#define py_is_classmethod(op) ((op)->type == &py_class_method_type)

/* bases should be NULL or tuple of classobjects! */
struct py_object* py_class_new(struct py_object*, struct py_object*);
struct py_object* py_classmember_new(struct py_object*);

struct py_object* py_classmethod_new(struct py_object*, struct py_object*);
struct py_object* py_classmethod_get_func(struct py_object*);
struct py_object* py_classmethod_get_self(struct py_object*);

#endif
