/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Execute compiled code */

#include <python/state.h>
#include <python/std.h>
#include <python/env.h>
#include <python/import.h>
#include <python/opcode.h>
#include <python/traceback.h>
#include <python/compile.h>
#include <python/ceval.h>
#include <python/errors.h>

#include <python/bltinmodule.h>

#include <python/frameobject.h>
#include <python/intobject.h>
#include <python/floatobject.h>
#include <python/dictobject.h>
#include <python/stringobject.h>
#include <python/listobject.h>
#include <python/tupleobject.h>
#include <python/methodobject.h>
#include <python/moduleobject.h>
#include <python/classobject.h>
#include <python/funcobject.h>

#include <apro.h>

struct py_object* py_get_locals(struct py_env* env) {
	if(env->current == NULL) return NULL;
	else return env->current->locals;
}

struct py_object* py_get_globals(struct py_env* env) {
	if(env->current == NULL) return NULL;
	else return env->current->globals;
}

/* TODO: Move all these "generalising" functions into their own place. */
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

static struct py_object* py_object_add(
		struct py_object* v, struct py_object* w) {

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

	py_error_set_string(py_type_error, "+ not supported by operands");
	return NULL;
}

static struct py_object* py_object_sub(
		struct py_object* v, struct py_object* w) {

	if((v->type == PY_TYPE_INT) && (w->type == PY_TYPE_INT)) {
		return py_int_new(py_int_get(v) - py_int_get(w));
	}
	else if((v->type == PY_TYPE_FLOAT) && (w->type == PY_TYPE_FLOAT)) {
		return py_float_new(py_float_get(v) - py_float_get(w));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for -");
	return NULL;
}

static struct py_object* py_object_mul(
		struct py_object* v, struct py_object* w) {

	if((v->type == PY_TYPE_INT) && (w->type == PY_TYPE_INT)) {
		return py_int_new(py_int_get(v) * py_int_get(w));
	}
	else if((v->type == PY_TYPE_FLOAT) && (w->type == PY_TYPE_FLOAT)) {
		return py_float_new(py_float_get(v) * py_float_get(w));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for *");
	return NULL;
}

static struct py_object* py_object_div(
		struct py_object* v, struct py_object* w) {

	if((v->type == PY_TYPE_INT) && (w->type == PY_TYPE_INT)) {
		return py_int_new(py_int_get(v) / py_int_get(w));
	}
	else if((v->type == PY_TYPE_FLOAT) && (w->type == PY_TYPE_FLOAT)) {
		return py_float_new(py_float_get(v) / py_float_get(w));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for /");
	return NULL;
}

static struct py_object* py_object_mod(
		struct py_object* v, struct py_object* w) {

	if((v->type == PY_TYPE_INT) && (w->type == PY_TYPE_INT)) {
		return py_int_new(py_int_get(v) % py_int_get(w));
	}
	else if((v->type == PY_TYPE_FLOAT) && (w->type == PY_TYPE_FLOAT)) {
		return py_float_new(fmod(py_float_get(v), py_float_get(w)));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for %");
	return NULL;
}

static struct py_object* py_object_neg(struct py_object* v) {
	if(v->type == PY_TYPE_INT) {
		return py_int_new(-py_int_get(v));
	}
	else if(v->type == PY_TYPE_FLOAT) {
		return py_float_new(-py_float_get(v));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for unary -");
	return NULL;
}

static struct py_object* py_object_not(struct py_object* v) {
	struct py_object* w = !py_object_truthy(v) ? PY_TRUE : PY_FALSE;

	return py_object_incref(w);
}

/*
 * TODO: This can possibly be less indirected using a distinct class init
 * 		 Syntax or like `class('name')'.
 */
static struct py_object* py_call_builtin(
		struct py_env* env, struct py_object* func, struct py_object* args) {

	if((func->type == PY_TYPE_METHOD)) {
		py_method_t method = ((struct py_method*) func)->method;
		struct py_object* self = ((struct py_method*) func)->self;

		return (*method)(env, self, args);
	}

	if(func->type == PY_TYPE_CLASS) {
		if(args != NULL) {
			py_error_set_string(
					py_type_error, "classobject() allows no arguments");
			return NULL;
		}

		return py_class_member_new(func);
	}

	py_error_set_string(py_type_error, "call of non-function");
	return NULL;
}

struct py_object* py_object_get_attr(struct py_object* v, const char* name) {
	if(v->type == PY_TYPE_CLASS_MEMBER) return py_class_member_get_attr(v, name);
	else if(v->type == PY_TYPE_CLASS) return py_class_get_attr(v, name);
	else if(v->type == PY_TYPE_MODULE) return py_module_get_attr(v, name);
	else {
		py_error_set_string(
				py_type_error,
				"can only get attributes on class, class member or module");
		return 0;
	}
}

static int py_object_set_attr(
		struct py_object* v, const char* name, struct py_object* w) {

	struct py_object* attr;

	if(v->type == PY_TYPE_CLASS_MEMBER) {
		attr = ((struct py_class_member*) v)->attr;
	}
	else if(v->type == PY_TYPE_MODULE) attr = ((struct py_module*) v)->attr;
	else {
		py_error_set_string(
				py_type_error,
				"can only set attributes on class_member or module");
		return -1;
	}

	if(w == NULL) return py_dict_remove(attr, name);
	else return py_dict_insert(attr, name, w);
}

struct py_object* py_call_function(
		struct py_env* env, struct py_object* func, struct py_object* args) {

	struct py_object* newarg = NULL;
	struct py_object* newlocals;
	struct py_object* newglobals;
	struct py_object* co;
	struct py_object* v;

	apro_stamp_start(APRO_CEVAL_CALL_RISING);

	if((func->type == PY_TYPE_CLASS_METHOD)) {
		struct py_object* self = py_class_method_get_self(func);
		func = py_class_method_get_func(func);

		if(args == NULL) args = self;
		else {
			newarg = py_tuple_new(2);

			if(newarg == NULL) return NULL;

			py_object_incref(self);
			py_object_incref(args);
			py_tuple_set(newarg, 0, self);
			py_tuple_set(newarg, 1, args);
			args = newarg;
		}
	}
	else if(!(func->type == PY_TYPE_FUNC)) {
		py_error_set_string(py_type_error, "call of non-function");
		return NULL;
	}

	co = ((struct py_func*) func)->code;

	if(!(co->type == PY_TYPE_CODE)) {
		/* TODO: Proper EH. */
		fprintf(stderr, "XXX Bad code\n");
		abort();
	}

	newlocals = py_dict_new();
	if(newlocals == NULL) {
		py_object_decref(newarg);
		return NULL;
	}

	newglobals = ((struct py_func*) func)->globals;
	py_object_incref(newglobals);

	apro_stamp_end(APRO_CEVAL_CALL_RISING);

	apro_stamp_start(APRO_CEVAL_CALL_EVAL);

	v = py_code_eval(env, (struct py_code*) co, newglobals, newlocals, args);

	apro_stamp_end(APRO_CEVAL_CALL_EVAL);

	py_object_decref(newlocals);
	py_object_decref(newglobals);

	py_object_decref(newarg);

	return v;
}

static struct py_object* py_object_ind(
		struct py_object* v, struct py_object* w) {

	py_ind_t ind;

	if((ind = py_types[v->type].ind)) {
		if(w->type != PY_TYPE_INT) {
			py_error_set_string(py_type_error, "sequence subscript not int");
			return NULL;
		}

		return ind(v, (unsigned) py_int_get(w));
	}
	else if(v->type == PY_TYPE_DICT) return py_dict_lookup_object(v, w);

	py_error_set_string(py_type_error, "unsubscriptable object");
	return NULL;
}

static struct py_object* py_loop_subscript(
		struct py_object* v, struct py_object* w) {

	unsigned i, n;

	if(!py_is_varobject(v)) {
		py_error_set_string(py_type_error, "loop over non-sequence");
		return NULL;
	}

	i = (unsigned) py_int_get(w);
	n = py_varobject_size(v);

	if(i >= n) return NULL; /* End of loop */

	return py_types[v->type].ind(v, i);
}

static int py_slice_index(struct py_object* v, unsigned* pi) {
	if(v != NULL) {
		if(!(v->type == PY_TYPE_INT)) {
			py_error_set_string(py_type_error, "slice index must be int");
			return -1;
		}

		*pi = (unsigned) py_int_get(v);
	}

	return 0;
}

/* return u[v:w] */
static struct py_object* py_apply_slice(
		struct py_object* u, struct py_object* v, struct py_object* w) {

	py_slice_t slice;
	unsigned ilow, ihigh;

	if(!(slice = py_types[u->type].slice)) {
		py_error_set_string(py_type_error, "only sequences can be sliced");
		return NULL;
	}

	ilow = 0;
	ihigh = py_varobject_size(u);

	if(py_slice_index(v, &ilow) != 0) return NULL;
	if(py_slice_index(w, &ihigh) != 0) return NULL;

	return slice(u, ilow, ihigh);
}

/* w[key] = v */
static int assign_subscript(
		struct py_object* w, struct py_object* key, struct py_object* v) {

	if(w->type == PY_TYPE_LIST) {
		unsigned i;

		if(!(key->type == PY_TYPE_INT)) {
			py_error_set_string(
					py_type_error, "sequence subscript must be integer");
			return -1;
		}

		i = (unsigned) py_int_get(key);
		if(i >= py_varobject_size(w)) {
			py_error_set_string(
					PY_INDEX_ERROR, "list assignment index out of range");
			return -1;
		}

		py_object_incref(v);
		py_object_decref(((struct py_list*) w)->item[i]);
		((struct py_list*) w)->item[i] = v;

		return 0;
	}
	else if((w->type == PY_TYPE_DICT)) return py_dict_assign(w, key, v);
	else {
		py_error_set_string(
				py_type_error, "can't assign to this subscripted object");
		return -1;
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
	if((w->type == PY_TYPE_STRING)) {
		const char* s;
		char c;

		if(!(v->type == PY_TYPE_STRING) || py_varobject_size(v) != 1) {
			py_error_set_string(
					py_type_error,
					"string member test needs char left operand");
			return -1;
		}

		c = py_string_get(v)[0];
		s = py_string_get(w);

		while(s < s + py_varobject_size(w)) if(c == *s++) return 1;

		return 0;
	}

	if(!py_is_varobject(w)) {
		py_error_set_string(
				py_type_error,
				"'in' or 'not in' needs sequence right argument");
		return -1;
	}

	n = py_varobject_size(w);

	for(i = 0; i < n; i++) {
		x = py_types[w->type].ind(w, i);
		cmp = py_object_cmp(v, x);
		py_object_decref(x);

		if(cmp == 0) return 1;
	}

	return 0;
}

static struct py_object* py_cmp_outcome(
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
			res = py_cmp_member(v, w);

			if(res < 0) return NULL;
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
				case PY_CMP_LT: res = cmp < 0;
					break;
				case PY_CMP_LE: res = cmp <= 0;
					break;
				case PY_CMP_EQ: res = cmp == 0;
					break;
				case PY_CMP_NE: res = cmp != 0;
					break;
				case PY_CMP_GT: res = cmp > 0;
					break;
				case PY_CMP_GE: res = cmp >= 0;
					break;
			}
		}
	}

	v = res ? PY_TRUE : PY_FALSE;
	py_object_incref(v);

	return v;
}

static int py_import_from(
		struct py_object* locals, struct py_object* v, const char* name) {

	struct py_object* w;
	struct py_object* x;

	w = ((struct py_module*) v)->attr;

	if(name[0] == '*') {
		unsigned i;
		unsigned n = py_dict_size(w);

		for(i = 0; i < n; i++) {
			name = py_dict_get_key(w, i);

			if(name == NULL || name[0] == '_') continue;

			x = py_dict_lookup(w, name);

			if(x == NULL) {
				/* TODO: can't happen? */
				py_error_set_string(py_name_error, name);
				return -1;
			}

			if(py_dict_insert(locals, name, x) != 0) return -1;
		}

		return 0;
	}
	else {
		x = py_dict_lookup(w, name);

		if(x == NULL) {
			py_error_set_string(py_name_error, name);
			return -1;
		}
		else { return py_dict_insert(locals, name, x); }
	}
}

/* Status code for main loop (reason for stack unwind) */

enum py_ceval_why {
	PY_WHY_NOT, /* No error */
	PY_WHY_EXCEPTION, /* Exception occurred */
	PY_WHY_RETURN, /* 'return' statement */
	PY_WHY_BREAK /* 'break' statement */
};

static const char* py_code_get_name(struct py_frame* f, unsigned i) {
	return py_string_get(py_list_get(f->code->names, i));
}

/* Interpreter main loop */

struct py_object* py_code_eval(
		struct py_env* env, struct py_code* co, struct py_object* globals,
		struct py_object* locals, struct py_object* args) {

	py_byte_t* code;
	py_byte_t* next;

	py_byte_t opcode; /* Current opcode */
	int oparg = 0; /* Current opcode argument, if any */

	struct py_object** stack_pointer;
	unsigned lineno = UINT_MAX; /* Current line number */

	struct py_object* x = PY_NONE; /* Result object -- NULL if error */
	struct py_object* v; /* Temporary objects popped off stack */
	struct py_object* w;
	struct py_object* u;

	struct py_frame* f; /* Current frame */

	struct py_object* retval = 0; /* Return value if why == PY_WHY_RETURN */
	enum py_ceval_why why = PY_WHY_NOT; /* Reason for block stack unwind */
	int err = 0; /* Error status -- nonzero if error */

	apro_stamp_start(APRO_CEVAL_CODE_EVAL_RISING);

	/* TODO: Why are these constants the random defaults. */
	f = py_frame_new(env->current, co, globals, locals, 50, 20);
	if(f == NULL) return NULL;

	env->current = f;
	code = f->code->code;
	next = code;
	stack_pointer = f->valuestack;

	if(args != NULL) {
		py_object_incref(args);
		*stack_pointer++ = args;
	}

	apro_stamp_end(APRO_CEVAL_CODE_EVAL_RISING);

	apro_stamp_start(APRO_CEVAL_CODE_EVAL);

	for(;;) {
		/* Extract opcode and argument */

		opcode = *next++;
		if(opcode >= PY_OP_HAVE_ARGUMENT) {
			next += 2;
			oparg = (next[-1] << 8) + next[-2];
		}

		/* Main switch on opcode */

		switch(opcode) {
			/*
			 * BEWARE!
			 * It is essential that any operation that fails sets either
			 * x to NULL, err to nonzero, or why to anything but PY_WHY_NOT,
			 * and that no operation that succeeds does this!
			 */

			case PY_OP_POP_TOP: {
				v = *--stack_pointer;
				py_object_decref(v);

				break;
			}

			case PY_OP_ROT_TWO: {
				v = *--stack_pointer;
				w = *--stack_pointer;
				*stack_pointer++ = v;
				*stack_pointer++ = (w);

				break;
			}

			case PY_OP_ROT_THREE: {
				v = *--stack_pointer;
				w = *--stack_pointer;
				x = *--stack_pointer;
				*stack_pointer++ = v;
				*stack_pointer++ = x;
				*stack_pointer++ = (w);

				break;
			}

			case PY_OP_DUP_TOP: {
				v = stack_pointer[-1];
				py_object_incref(v);
				*stack_pointer++ = v;

				break;
			}

			case PY_OP_UNARY_NEGATIVE: {
				v = *--stack_pointer;
				x = py_object_neg(v);
				py_object_decref(v);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_UNARY_NOT: {
				v = *--stack_pointer;
				x = py_object_not(v);
				py_object_decref(v);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_UNARY_CALL: {
				v = *--stack_pointer;

				/* TODO: Can these be consolidated to avoid the branch? */
				if(v->type == PY_TYPE_CLASS_METHOD ||
					v->type == PY_TYPE_FUNC) {

					x = py_call_function(env, v, NULL);
				}
				else x = py_call_builtin(env, v, NULL);

				py_object_decref(v);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_MULTIPLY: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_mul(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_DIVIDE: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_div(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_MODULO: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_mod(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_ADD: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_add(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_SUBTRACT: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_sub(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_SUBSCR: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_object_ind(v, w);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BINARY_CALL: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(v->type == PY_TYPE_CLASS_METHOD ||
					v->type == PY_TYPE_FUNC) {

					x = py_call_function(env, v, w);
				}
				else x = py_call_builtin(env, v, w);

				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_SLICE + 0: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_SLICE + 1: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_SLICE + 2: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_SLICE + 3: {
				if((opcode - PY_OP_SLICE) & 2) { w = *--stack_pointer; }
				else { w = NULL; }

				if((opcode - PY_OP_SLICE) & 1) { v = *--stack_pointer; }
				else { v = NULL; }

				u = *--stack_pointer;
				x = py_apply_slice(u, v, w);
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_STORE_SUBSCR: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				u = *--stack_pointer;
				/* v[w] = u */
				err = assign_subscript(v, w, u);
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			/* TODO: De-printify opcodes. */
			case PY_OP_PRINT_EXPR: {
				v = *--stack_pointer;
				py_object_decref(v);
				break;
			}

			case PY_OP_BREAK_LOOP: {
				why = PY_WHY_BREAK;

				break;
			}

			case PY_OP_LOAD_LOCALS: {
				v = f->locals;
				py_object_incref(v);
				*stack_pointer++ = v;

				break;
			}

			case PY_OP_RETURN_VALUE: {
				retval = *--stack_pointer;
				why = PY_WHY_RETURN;

				break;
			}

			/* TODO: Should these be a concern? Seems legacy. */
			case PY_OP_REQUIRE_ARGS: {
				if(!(stack_pointer - f->valuestack)) {
					py_error_set_string(
							py_type_error, "function expects argument(s)");
					why = PY_WHY_EXCEPTION;
				}

				break;
			}
			case PY_OP_REFUSE_ARGS: {
				if((stack_pointer - f->valuestack)) {
					py_error_set_string(
							py_type_error, "function expects no argument(s)");
					why = PY_WHY_EXCEPTION;
				}

				break;
			}

			case PY_OP_BUILD_FUNCTION: {
				v = *--stack_pointer;
				x = py_func_new(v, f->globals);
				py_object_decref(v);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_POP_BLOCK: {
				struct py_block* b = py_block_pop(f);

				while((stack_pointer - f->valuestack) > (int) b->level) {
					v = *--stack_pointer;
					py_object_decref(v);
				}

				break;
			}

			case PY_OP_BUILD_CLASS: {
				v = *--stack_pointer;
				x = py_class_new(v);
				*stack_pointer++ = x;
				py_object_decref(v);

				break;
			}

			case PY_OP_STORE_NAME: {
				v = *--stack_pointer;
				err = py_dict_insert(f->locals, py_code_get_name(f, oparg), v);
				py_object_decref(v);

				break;
			}

			case PY_OP_UNPACK_TUPLE: {
				v = *--stack_pointer;

				if(!(v->type == PY_TYPE_TUPLE)) {
					py_error_set_string(py_type_error, "unpack non-tuple");
					why = PY_WHY_EXCEPTION;
				}
				else if(py_varobject_size(v) != (unsigned) oparg) {
					py_error_set_string(
							py_runtime_error, "unpack tuple of wrong size");
					why = PY_WHY_EXCEPTION;
				}
				else {
					for(; --oparg >= 0;) {
						w = py_tuple_get(v, oparg);
						py_object_incref(w);
						*stack_pointer++ = (w);
					}
				}

				py_object_decref(v);

				break;
			}

			/* TODO: Consolidate list/tuple since they do the same thing? */
			case PY_OP_UNPACK_LIST: {
				v = *--stack_pointer;
				if(!(v->type == PY_TYPE_LIST)) {
					py_error_set_string(py_type_error, "unpack non-list");
					why = PY_WHY_EXCEPTION;
				}
				else if(py_varobject_size(v) != (unsigned) oparg) {
					py_error_set_string(
							py_runtime_error, "unpack list of wrong size");
					why = PY_WHY_EXCEPTION;
				}
				else {
					for(; --oparg >= 0;) {
						w = py_list_get(v, oparg);
						py_object_incref(w);
						*stack_pointer++ = (w);
					}
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_STORE_ATTR: {
				v = *--stack_pointer;
				u = *--stack_pointer;
				/* v.name = u */
				err = py_object_set_attr(v, py_code_get_name(f, oparg), u);
				py_object_decref(v);
				py_object_decref(u);

				break;
			}

			case PY_OP_LOAD_CONST: {
				x = py_list_get(f->code->consts, oparg);
				py_object_incref(x);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_LOAD_NAME: {
				const char* name = py_code_get_name(f, oparg);
				x = py_dict_lookup(f->locals, name);
				if(x == NULL) {
					x = py_dict_lookup(f->globals, name);
					if(x == NULL) x = py_builtin_get(name);
				}

				if(x == NULL) py_error_set_string(py_name_error, name);
				else py_object_incref(x);

				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BUILD_TUPLE: {
				x = py_tuple_new(oparg);

				if(x != NULL) {
					for(; --oparg >= 0;) {
						w = *--stack_pointer;
						err = py_tuple_set(x, oparg, w);

						if(err != 0) break;
					}

					*stack_pointer++ = x;
				}

				break;
			}

			case PY_OP_BUILD_LIST: {
				x = py_list_new(oparg);

				if(x != NULL) {
					for(; --oparg >= 0;) {
						w = *--stack_pointer;
						err = py_list_set(x, oparg, w);

						if(err != 0) break;
					}

					*stack_pointer++ = x;
				}

				break;
			}

			case PY_OP_BUILD_MAP: {
				x = py_dict_new();
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_LOAD_ATTR: {
				v = *--stack_pointer;
				x = py_object_get_attr(v, py_code_get_name(f, oparg));
				py_object_decref(v);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_COMPARE_OP: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				x = py_cmp_outcome((enum py_cmp_op) oparg, v, w);

				py_object_decref(v);
				py_object_decref(w);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_IMPORT_NAME: {
				x = py_import_module(env, py_code_get_name(f, oparg));
				py_object_incref(x);
				*stack_pointer++ = x;

				break;
			}

			case PY_OP_IMPORT_FROM: {
				v = stack_pointer[-1];
				err = py_import_from(f->locals, v, py_code_get_name(f, oparg));

				break;
			}

			case PY_OP_JUMP_FORWARD: {
				next += oparg;

				break;
			}

			case PY_OP_JUMP_IF_FALSE: {
				if(!py_object_truthy(stack_pointer[-1])) next += oparg;

				break;
			}

			case PY_OP_JUMP_IF_TRUE: {
				if(py_object_truthy(stack_pointer[-1])) next += oparg;

				break;
			}

			case PY_OP_JUMP_ABSOLUTE: {
				next = code + oparg;
				break;
			}

			case PY_OP_FOR_LOOP:
				/* for v in s: ...
				On entry: stack contains s, i.
				On exit: stack contains s, i+1, s[i];
				but if loop exhausted:
				s, i are popped, and we jump */
				w = *--stack_pointer; /* Loop index */
				v = *--stack_pointer; /* Sequence struct py_object*/
				u = py_loop_subscript(v, w);
				if(u != NULL) {
					*stack_pointer++ = v;
					x = py_int_new(py_int_get(w) + 1);
					*stack_pointer++ = x;
					py_object_decref(w);
					*stack_pointer++ = (u);
				}
				else {
					py_object_decref(v);
					py_object_decref(w);
					/* A NULL can mean "s exhausted" but also an error: */
					if(py_error_occurred()) why = PY_WHY_EXCEPTION;
					else next += oparg;
				}
				break;

			case PY_OP_SETUP_LOOP: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_SETUP_EXCEPT: {
				py_block_setup(
						f, opcode, (next - code) + oparg,
						(stack_pointer - f->valuestack));

				break;
			}

			case PY_OP_SET_LINENO: {
				lineno = oparg;

				break;
			}

			default: {
				py_error_set_string(
						py_system_error, "py_code_eval: unknown opcode");
				why = PY_WHY_EXCEPTION;

				break;
			}
		} /* switch */


		/* Quickly continue if no error occurred */

		if(why == PY_WHY_NOT) {
			if(err == 0 && x != NULL) continue; /* Normal, fast path */

			why = PY_WHY_EXCEPTION;
			x = PY_NONE;
			err = 0;
		}

#ifndef NDEBUG
		/* Double-check exception status */

		if(why == PY_WHY_EXCEPTION) {
			if(!py_error_occurred()) {
				py_error_set_string(py_system_error, "ghost error");
				why = PY_WHY_EXCEPTION;
			}
		}
		else if(py_error_occurred()) why = PY_WHY_EXCEPTION;
#endif

		/* Log traceback info if this is a real exception */

		if(why == PY_WHY_EXCEPTION) py_traceback_new(f, lineno);

		/* Unwind stacks if a (pseudo) exception occurred */

		while(f->iblock > 0) {
			struct py_block* b = py_block_pop(f);
			while((stack_pointer - f->valuestack) > (int) b->level) {
				v = *--stack_pointer;
				py_object_decref(v);
			}

			if(b->type == PY_OP_SETUP_LOOP && why == PY_WHY_BREAK) {
				why = PY_WHY_NOT;
				next = code + b->handler;
				break;
			}
		} /* unwind stack */

		/* End the loop if we still have an error (or return) */

		if(why != PY_WHY_NOT) break;
	} /* main loop */

	apro_stamp_end(APRO_CEVAL_CODE_EVAL);

	apro_stamp_start(APRO_CEVAL_CODE_EVAL_FALLING);

	/* Pop remaining stack entries */

	while((stack_pointer - f->valuestack)) {
		v = *--stack_pointer;
		py_object_decref(v);
	}

	/* Restore previous frame and release the current one */

	env->current = f->back;
	py_object_decref(f);

	apro_stamp_end(APRO_CEVAL_CODE_EVAL_FALLING);

	if(why == PY_WHY_RETURN) return retval;
	else return NULL;
}
