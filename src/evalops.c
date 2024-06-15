/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#include <python/evalops.h>

#include <python/object/int.h>
#include <python/object/float.h>
#include <python/object/dict.h>

/* Test a value used as condition, e.g., in a for or if statement */
int py_object_truthy(struct py_object* v) {
	if(v->type == PY_TYPE_INT) return py_int_get(v) != 0;
	else if(v->type == PY_TYPE_FLOAT) return py_float_get(v) != 0.0;
	else if(py_is_varobject(v)) return py_varobject_size(v) != 0;
	else if(v->type == PY_TYPE_DICT) return ((struct py_dict*) v)->used != 0;
	else if(v == PY_NONE) return 0;

	/* All other objects are 'true' */
	return 1;
}

struct py_object* py_object_add(struct py_object* v, struct py_object* w) {
	py_cat_t cat;

	if(v->type == PY_TYPE_INT && w->type == PY_TYPE_INT) {
		return py_int_new(py_int_get(v) + py_int_get(w));
	}
	else if(v->type == PY_TYPE_FLOAT && w->type == PY_TYPE_FLOAT) {
		return py_float_new(py_float_get(v) + py_float_get(w));
	}
	else if((cat = py_types[v->type].cat)) {
		return cat(v, w);
	}

	return 0;
}

struct py_object* py_object_sub(struct py_object* v, struct py_object* w) {
	if(v->type == PY_TYPE_INT && w->type == PY_TYPE_INT) {
		return py_int_new(py_int_get(v) - py_int_get(w));
	}
	else if(v->type == PY_TYPE_FLOAT && w->type == PY_TYPE_FLOAT) {
		return py_float_new(py_float_get(v) - py_float_get(w));
	}

	return 0;
}
