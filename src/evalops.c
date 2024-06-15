/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#include <python/evalops.h>
#include <python/env.h>
#include <python/ceval.h>

#include <python/object/int.h>
#include <python/object/func.h>
#include <python/object/tuple.h>
#include <python/object/float.h>
#include <python/object/dict.h>
#include <python/object/module.h>
#include <python/object/string.h>
#include <python/object/list.h>
#include <python/object/class.h>

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

struct py_object* py_object_not(struct py_object* v) {
	return py_object_incref(!py_object_truthy(v) ? PY_TRUE : PY_FALSE);
}

struct py_object* py_object_neg(struct py_object* v) {
	if(v->type == PY_TYPE_INT) return py_int_new(-py_int_get(v));
	else if(v->type == PY_TYPE_FLOAT) return py_float_new(-py_float_get(v));

	return 0;
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

struct py_object* py_object_mul(struct py_object* v, struct py_object* w) {
	if(v->type == PY_TYPE_INT && w->type == PY_TYPE_INT) {
		return py_int_new(py_int_get(v) * py_int_get(w));
	}
	else if(v->type == PY_TYPE_FLOAT && w->type == PY_TYPE_FLOAT) {
		return py_float_new(py_float_get(v) * py_float_get(w));
	}

	return 0;
}


struct py_object* py_object_div(struct py_object* v, struct py_object* w) {
	if(v->type == PY_TYPE_INT && w->type == PY_TYPE_INT) {
		return py_int_new(py_int_get(v) / py_int_get(w));
	}
	else if(v->type == PY_TYPE_FLOAT && w->type == PY_TYPE_FLOAT) {
		return py_float_new(py_float_get(v) / py_float_get(w));
	}

	return 0;
}

struct py_object* py_object_mod(struct py_object* v, struct py_object* w) {
	if(v->type == PY_TYPE_INT && w->type == PY_TYPE_INT) {
		return py_int_new(py_int_get(v) % py_int_get(w));
	}
	else if(v->type == PY_TYPE_FLOAT && w->type == PY_TYPE_FLOAT) {
		return py_float_new(fmod(py_float_get(v), py_float_get(w)));
	}

	return 0;
}

int py_assign_subscript(
		struct py_object* op, struct py_object* key, struct py_object* value) {

	if(op->type == PY_TYPE_LIST) {
		unsigned i;
		struct py_list* lp = (void*) op;

		if(key->type != PY_TYPE_INT) return -1;

		if((i = py_int_get(key)) >= py_varobject_size(op)) return -1;

		py_object_decref(lp->item[i]);
		lp->item[i] = py_object_incref(value);

		return 0;
	}
	else if(op->type == PY_TYPE_DICT) {
		if(key->type != PY_TYPE_STRING) return -1;

		return py_dict_assign(op, key, value);
	}

	return -1;
}

struct py_object* py_object_ind(struct py_object* v, struct py_object* w) {
	py_ind_t ind;

	if((ind = py_types[v->type].ind)) {
		if(w->type != PY_TYPE_INT) return 0;

		return ind(v, py_int_get(w));
	}
	else if(v->type == PY_TYPE_DICT) return py_dict_lookup_object(v, w);

	return 0;
}

struct py_object* py_apply_slice(
		struct py_object* u, struct py_object* v, struct py_object* w) {

	py_slice_t slice;
	unsigned low, high;

	if(!(slice = py_types[u->type].slice)) return 0;

	low = 0;
	high = py_varobject_size(u);

	if(py_slice_index(v, &low) != 0) return 0;
	if(py_slice_index(w, &high) != 0) return 0;

	return slice(u, low, high);
}

int py_slice_index(struct py_object* v, unsigned* pi) {
	if(!v) return 0;

	if(v->type != PY_TYPE_INT) return -1;

	*pi = py_int_get(v);

	return 0;
}

struct py_object* py_loop_subscript(struct py_object* v, struct py_object* w) {
	unsigned i = py_int_get(w);
	unsigned n = py_varobject_size(v);

	if(i >= n) return 0; /* End of loop */

	return py_types[v->type].ind(v, i);
}

struct py_object* py_object_get_attr(struct py_object* v, const char* name) {
	switch(v->type) {
		default: return 0;

		case PY_TYPE_CLASS_MEMBER: return py_class_member_get_attr(v, name);
		case PY_TYPE_CLASS: return py_class_get_attr(v, name);
		case PY_TYPE_MODULE: return py_module_get_attr(v, name);
	}
}

int py_object_set_attr(
		struct py_object* v, const char* name, struct py_object* w) {

	struct py_object* attr;

	if(v->type == PY_TYPE_CLASS_MEMBER) {
		attr = ((struct py_class_member*) v)->attr;
	}
	else if(v->type == PY_TYPE_MODULE) attr = ((struct py_module*) v)->attr;
	else return -1;

	if(!w) return py_dict_remove(attr, name);
	else return py_dict_insert(attr, name, w);
}

/*
 * TODO: This can be less indirected using a distinct class init
 * 		 Syntax or like `class('name')'.
 */
struct py_object* py_call_function(
		struct py_env* env, struct py_object* func, struct py_object* args) {

	struct py_object* arglist = NULL;

	switch(func->type) {
		default: return 0;

		case PY_TYPE_METHOD: {
			py_method_t method = ((struct py_method*) func)->method;
			struct py_object* self = ((struct py_method*) func)->self;

			return (*method)(env, self, args);
		}

		case PY_TYPE_CLASS: {
			if(args) return 0;
			return py_class_member_new(func);
		}

		case PY_TYPE_CLASS_METHOD: {
			struct py_object* self = py_class_method_get_self(func);
			func = py_class_method_get_func(func);

			if(!args) args = self;
			else {
				if(!(arglist = py_tuple_new(2))) return 0;

				py_tuple_set(arglist, 0, py_object_incref(self));
				py_tuple_set(arglist, 1, py_object_incref(args));

				args = arglist;
			}

			PY_FALLTHROUGH;
			/* FALLTHRU */
		}

		case PY_TYPE_FUNC: {
			struct py_object* locals;
			struct py_object* globals;
			struct py_object* retval;

			if(!(locals = py_dict_new())) {
				py_object_decref(arglist);
				return 0;
			}

			globals = py_object_incref(((struct py_func*) func)->globals);

			retval = py_code_eval(
					env, (void*) ((struct py_func*) func)->code, globals,
					locals, args);

			py_object_decref(locals);
			py_object_decref(globals);
			py_object_decref(arglist);

			return retval;
		}
	}
}

static int py_cmp_exception(struct py_object* err, struct py_object* v) {
	if(v->type == PY_TYPE_TUPLE) {
		unsigned i, n;

		n = py_varobject_size(v);
		for(i = 0; i < n; i++) if(err == py_tuple_get(v, i)) return 1;

		return 0;
	}

	return err == v;
}

static int py_cmp_member(struct py_object* v, struct py_object* w) {
	struct py_object* x;
	unsigned i, n;
	int cmp;

	/* Special case for char in string */
	if(w->type == PY_TYPE_STRING) {
		const char* s;
		char c;

		if(v->type != PY_TYPE_STRING || py_varobject_size(v) != 1) return -1;

		c = py_string_get(v)[0];
		s = py_string_get(w);

		while(s < s + py_varobject_size(w)) if(c == *s++) return 1;

		return 0;
	}

	if(!py_is_varobject(w)) return -1;

	n = py_varobject_size(w);

	for(i = 0; i < n; i++) {
		x = py_types[w->type].ind(w, i);
		cmp = py_object_cmp(v, x);
		py_object_decref(x);

		if(cmp == 0) return 1;
	}

	return 0;
}

struct py_object* py_cmp_outcome(
		enum py_cmp_op op, struct py_object* v, struct py_object* w) {

	int cmp;
	int res = 0;

	switch(op) {
		case PY_CMP_IS: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_CMP_IS_NOT: {
			res = (v == w);
			if(op == PY_CMP_IS_NOT) res = !res;

			break;
		}

		case PY_CMP_IN: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_CMP_NOT_IN: {
			if(!py_is_varobject(w)) return 0;

			res = py_cmp_member(v, w);

			if(res < 0) return 0;
			if(op == PY_CMP_NOT_IN) res = !res;

			break;
		}

		case PY_CMP_EXC_MATCH: {
			res = py_cmp_exception(v, w);
			break;
		}

		default: {
			cmp = py_object_cmp(v, w);

			switch(op) {
				default: break;

				case PY_CMP_LT: res = cmp < 0; break;
				case PY_CMP_LE: res = cmp <= 0; break;
				case PY_CMP_EQ: res = cmp == 0; break;
				case PY_CMP_NE: res = cmp != 0; break;
				case PY_CMP_GT: res = cmp > 0; break;
				case PY_CMP_GE: res = cmp >= 0; break;
			}
		}
	}

	return py_object_incref(res ? PY_TRUE : PY_FALSE);
}

int py_import_from(
		struct py_object* locals, struct py_object* v, const char* name) {

	struct py_object* x;
	struct py_object* w = ((struct py_module*) v)->attr;

	if(name[0] == '*') {
		unsigned i;

		for(i = 0; i < py_dict_size(w); i++) {
			name = py_dict_get_key(w, i);

			if(!name || name[0] == '_') continue;

			if(!(x = py_dict_lookup(w, name))) return -1;

			if(py_dict_insert(locals, name, x) != 0) return -1;
		}

		return 0;
	}

	if(!(x = py_dict_lookup(w, name))) return -1;

	return py_dict_insert(locals, name, x);
}
