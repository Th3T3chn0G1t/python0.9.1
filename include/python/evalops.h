/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_EVALOPS_H
#define PY_EVALOPS_H

#include <python/object.h>
#include <python/opcode.h>

struct py_env;

int py_object_truthy(struct py_object*);

struct py_object* py_object_not(struct py_object*);

struct py_object* py_object_neg(struct py_object*);

struct py_object* py_object_add(struct py_object*, struct py_object*);
struct py_object* py_object_sub(struct py_object*, struct py_object*);

struct py_object* py_object_mul(struct py_object*, struct py_object*);
struct py_object* py_object_div(struct py_object*, struct py_object*);
struct py_object* py_object_mod(struct py_object*, struct py_object*);

/* w[key] = v */
int py_assign_subscript(
		struct py_object*, struct py_object*, struct py_object*);

struct py_object* py_object_ind(struct py_object*, struct py_object*);

/* return u[v:w] */
struct py_object* py_apply_slice(
		struct py_object*, struct py_object*, struct py_object*);

int py_slice_index(struct py_object*, unsigned*);

struct py_object* py_loop_subscript(struct py_object*, struct py_object*);

struct py_object* py_object_get_attr(struct py_object*, const char*);

/* v.name = u */
int py_object_set_attr(struct py_object*, const char*, struct py_object*);

struct py_object* py_call_function(
		struct py_env*, struct py_object*, struct py_object*);

struct py_object* py_cmp_outcome(
		enum py_cmp_op, struct py_object*, struct py_object*);

int py_import_from(struct py_object*, struct py_object*, const char*);

#endif
