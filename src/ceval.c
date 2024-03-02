/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Execute compiled code */

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
#include <python/listobject.h>
#include <python/tupleobject.h>
#include <python/methodobject.h>
#include <python/moduleobject.h>
#include <python/classobject.h>
#include <python/funcobject.h>

struct py_frame* py_frame_current;

#ifndef NDEBUG
# define TRACE
#endif

#ifdef TRACE
static int prtrace(struct py_object* v, char* str, int trace) {
	if(!trace) return 0;

	printf("%s ", str);
	py_object_print(v, stdout, PY_PRINT_NORMAL);
	printf("\n");

	return 0;
}
#endif

struct py_object* py_get_locals(void) {
	if(py_frame_current == NULL) return NULL;
	else return py_frame_current->locals;
}

struct py_object* py_get_globals(void) {
	if(py_frame_current == NULL) return NULL;
	else return py_frame_current->globals;
}

/* TODO: Move all these "generalising" functions into their own place. */
/* Test a value used as condition, e.g., in a for or if statement */
static int py_object_truthy(struct py_object* v) {
	if(py_is_int(v)) return py_int_get(v) != 0;
	if(py_is_float(v)) return py_float_get(v) != 0.0;

	if(v->type->sequencemethods != NULL) {
		return py_varobject_size(v) != 0;
	}

	if(v->type->mappingmethods != NULL) {
		return (*v->type->mappingmethods->len)(v) != 0;
	}

	if(v == PY_NONE) return 0;

	/* All other objects are 'true' */
	return 1;
}

static struct py_object* py_object_add(
		struct py_object* v, struct py_object* w) {

	if(v->type->numbermethods != NULL) {
		v = (*v->type->numbermethods->add)(v, w);
	}
	else if(v->type->sequencemethods != NULL) {
		v = (*v->type->sequencemethods->cat)(v, w);
	}
	else {
		py_error_set_string(py_type_error, "+ not supported by operands");
		return NULL;
	}

	return v;
}

static struct py_object* py_object_sub(
		struct py_object* v, struct py_object* w) {

	if(v->type->numbermethods != NULL) {
		return (*v->type->numbermethods->sub)(v, w);
	}

	py_error_set_string(py_type_error, "bad operand type(s) for -");
	return NULL;
}

static struct py_object* py_object_mul(
		struct py_object* v, struct py_object* w) {

	struct py_type* tp;

	if(py_is_int(v) && w->type->sequencemethods != NULL) {
		/* int*sequence -- swap v and w */
		struct py_object* tmp = v;

		v = w;
		w = tmp;
	}

	tp = v->type;

	if(tp->numbermethods != NULL) return (*tp->numbermethods->mul)(v, w);

	if(tp->sequencemethods != NULL) {
		if(!py_is_int(w)) {
			py_error_set_string(
					py_type_error, "can't mul sequence with non-int");
			return NULL;
		}

		if(tp->sequencemethods->rep == NULL) {
			py_error_set_string(py_type_error, "sequence does not support *");
			return NULL;
		}

		return (*tp->sequencemethods->rep)(v, (int) py_int_get(w));
	}

	py_error_set_string(py_type_error, "bad operand type(s) for *");
	return NULL;
}

static struct py_object* py_object_div(struct py_object* v, struct py_object* w) {
	if(v->type->numbermethods != NULL) {
		return (*v->type->numbermethods->div)(v, w);
	}

	py_error_set_string(py_type_error, "bad operand type(s) for /");
	return NULL;
}

static struct py_object* py_object_mod(struct py_object* v, struct py_object* w) {
	if(v->type->numbermethods != NULL) {
		return (*v->type->numbermethods->mod)(v, w);
	}

	py_error_set_string(py_type_error, "bad operand type(s) for %");
	return NULL;
}

static struct py_object* py_object_neg(struct py_object* v) {
	if(v->type->numbermethods != NULL) {
		return (*v->type->numbermethods->neg)(v);
	}

	py_error_set_string(py_type_error, "bad operand type(s) for unary -");
	return NULL;
}

static struct py_object* py_object_pos(struct py_object* v) {
	if(v->type->numbermethods != NULL) {
		return (*v->type->numbermethods->pos)(v);
	}

	py_error_set_string(py_type_error, "bad operand type(s) for unary +");
	return NULL;
}

static struct py_object* py_object_not(struct py_object* v) {
	int outcome = py_object_truthy(v);
	struct py_object* w = outcome == 0 ? PY_TRUE : PY_FALSE;

	py_object_incref(w);
	return w;
}

static struct py_object*
call_builtin(struct py_object* func, struct py_object* arg) {
	if(py_is_method(func)) {
		py_method_t meth = py_method_get(func);
		struct py_object* self = py_method_get_self(func);

		return (*meth)(self, arg);
	}

	if(py_is_class(func)) {
		if(arg != NULL) {
			py_error_set_string(
					py_type_error, "classobject() allows no arguments");
			return NULL;
		}

		return py_classmember_new(func);
	}

	py_error_set_string(py_type_error, "call of non-function");
	return NULL;
}

struct py_object*
py_call_function(struct py_object* func, struct py_object* arg) {
	struct py_object* newarg = NULL;
	struct py_object* newlocals;
	struct py_object* newglobals;
	struct py_object* co;
	struct py_object* v;

	if(py_is_classmethod(func)) {
		struct py_object* self = py_classmethod_get_self(func);
		func = py_classmethod_get_func(func);

		if(arg == NULL) arg = self;
		else {
			newarg = py_tuple_new(2);

			if(newarg == NULL) return NULL;

			py_object_incref(self);
			py_object_incref(arg);
			py_tuple_set(newarg, 0, self);
			py_tuple_set(newarg, 1, arg);
			arg = newarg;
		}
	}
	else if(!py_is_func(func)) {
		py_error_set_string(py_type_error, "call of non-function");
		return NULL;
	}

	co = py_func_get_code(func);
	if(co == NULL) {
		py_object_decref(newarg);
		return NULL;
	}

	if(!py_is_code(co)) {
		fprintf(stderr, "XXX Bad code\n");
		abort();
	}

	newlocals = py_dict_new();
	if(newlocals == NULL) {
		py_object_decref(newarg);
		return NULL;
	}

	newglobals = py_func_get_globals(func);
	py_object_incref(newglobals);

	v = py_code_eval((struct py_code*) co, newglobals, newlocals, arg);

	py_object_decref(newlocals);
	py_object_decref(newglobals);

	py_object_decref(newarg);

	return v;
}

static struct py_object*
py_object_ind(struct py_object* v, struct py_object* w) {
	struct py_type* tp = v->type;

	if(tp->sequencemethods == NULL && tp->mappingmethods == NULL) {
		py_error_set_string(py_type_error, "unsubscriptable object");
		return NULL;
	}

	if(tp->sequencemethods != NULL) {
		int i;

		if(!py_is_int(w)) {
			py_error_set_string(py_type_error, "sequence subscript not int");
			return NULL;
		}

		i = py_int_get(w);
		return (*tp->sequencemethods->ind)(v, i);
	}

	return (*tp->mappingmethods->ind)(v, w);
}

static struct py_object*
loop_subscript(struct py_object* v, struct py_object* w) {
	struct py_sequencemethods* sq = v->type->sequencemethods;
	int i, n;

	if(sq == NULL) {
		py_error_set_string(py_type_error, "loop over non-sequence");
		return NULL;
	}

	i = py_int_get(w);
	n = py_varobject_size(v);

	if(i >= n) return NULL; /* End of loop */

	return (*sq->ind)(v, i);
}

static int slice_index(struct py_object* v, int isize, int* pi) {
	if(v != NULL) {
		if(!py_is_int(v)) {
			py_error_set_string(py_type_error, "slice index must be int");
			return -1;
		}

		*pi = py_int_get(v);
		if(*pi < 0) *pi += isize;
	}

	return 0;
}

/* return u[v:w] */
static struct py_object*
apply_slice(struct py_object* u, struct py_object* v, struct py_object* w) {
	struct py_type* tp = u->type;
	int ilow, ihigh, isize;

	if(tp->sequencemethods == NULL) {
		py_error_set_string(py_type_error, "only sequences can be sliced");
		return NULL;
	}

	ilow = 0;
	isize = ihigh = py_varobject_size(u);

	if(slice_index(v, isize, &ilow) != 0) return NULL;
	if(slice_index(w, isize, &ihigh) != 0) return NULL;

	return (*tp->sequencemethods->slice)(u, ilow, ihigh);
}

/* w[key] = v */
static int assign_subscript(
		struct py_object* w, struct py_object* key, struct py_object* v) {
	struct py_type* tp = w->type;
	struct py_sequencemethods* sq;
	struct py_mappingmethods* mp;
	int (* func)();

	sq = tp->sequencemethods;
	mp = tp->mappingmethods;

	if(sq != NULL && (func = sq->assign_item) != NULL) {
		if(!py_is_int(key)) {
			py_error_set_string(
					py_type_error, "sequence subscript must be integer");
			return -1;
		}

		else { return (*func)(w, (int) py_int_get(key), v); }
	}
	else if(mp != NULL && (func = mp->assign) != NULL) {
		return (*func)(w, key, v);
	}
	else {
		py_error_set_string(
				py_type_error, "can't assign to this subscripted object");
		return -1;
	}
}

/* u[v:w] = x */
static int assign_slice(
		struct py_object* u, struct py_object* v, struct py_object* w,
		struct py_object* x) {
	struct py_sequencemethods* sq = u->type->sequencemethods;
	int ilow, ihigh, isize;

	if(sq == NULL) {
		py_error_set_string(py_type_error, "assign to slice of non-sequence");
		return -1;
	}

	if(sq->assign_slice == NULL) {
		py_error_set_string(py_type_error, "unassignable slice");
		return -1;
	}

	ilow = 0;
	isize = ihigh = py_varobject_size(u);

	if(slice_index(v, isize, &ilow) != 0) return -1;
	if(slice_index(w, isize, &ihigh) != 0) return -1;

	return (*sq->assign_slice)(u, ilow, ihigh, x);
}

static int cmp_exception(struct py_object* err, struct py_object* v) {
	if(py_is_tuple(v)) {
		int i, n;

		n = py_varobject_size(v);
		for(i = 0; i < n; i++) {
			if(err == py_tuple_get(v, i)) return 1;
		}

		return 0;
	}

	return err == v;
}

static int cmp_member(struct py_object* v, struct py_object* w) {
	int i, n, cmp;
	struct py_object* x;
	struct py_sequencemethods* sq;

	/* Special case for char in string */
	if(py_is_string(w)) {
		char* s;
		char* end;
		char c;

		if(!py_is_string(v) || py_varobject_size(v) != 1) {
			py_error_set_string(
					py_type_error,
					"string member test needs char left operand");
			return -1;
		}

		c = py_string_get_value(v)[0];
		s = py_string_get_value(w);
		end = s + py_varobject_size(w);

		while(s < end) {
			if(c == *s++) return 1;
		}

		return 0;
	}

	sq = w->type->sequencemethods;

	if(sq == NULL) {
		py_error_set_string(
				py_type_error,
				"'in' or 'not in' needs sequence right argument");
		return -1;
	}

	n = py_varobject_size(w);

	for(i = 0; i < n; i++) {
		x = (*sq->ind)(w, i);
		cmp = py_object_cmp(v, x);
		py_object_decref(x);

		if(cmp == 0) return 1;
	}

	return 0;
}

static struct py_object*
cmp_outcome(enum py_cmp_op op, struct py_object* v, struct py_object* w) {
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
			res = cmp_member(v, w);

			if(res < 0) return NULL;
			if(op == PY_CMP_NOT_IN) res = !res;

			break;
		}

		case PY_CMP_EXC_MATCH: {
			res = cmp_exception(v, w);
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
					/* XXX no default? (res is initialized to 0 though) */
			}
		}
	}

	v = res ? PY_TRUE : PY_FALSE;
	py_object_incref(v);

	return v;
}

static int
import_from(struct py_object* locals, struct py_object* v, char* name) {
	struct py_object* w;
	struct py_object* x;

	w = py_module_get_dict(v);

	if(name[0] == '*') {
		int i;
		int n = py_dict_size(w);

		for(i = 0; i < n; i++) {
			name = py_dict_get_key(w, i);

			if(name == NULL || name[0] == '_') continue;

			x = py_dict_lookup(w, name);

			if(x == NULL) {
				/* XXX can't happen? */
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

static struct py_object* build_class(struct py_object* v, struct py_object* w) {
	if(py_is_tuple(v)) {
		int i;

		for(i = py_varobject_size(v); --i >= 0;) {
			struct py_object* x = py_tuple_get(v, i);

			if(!py_is_class(x)) {
				py_error_set_string(
						py_type_error, "base is not a class object");
				return NULL;
			}
		}
	}
	else { v = NULL; }

	if(!py_is_dict(w)) {
		py_error_set_string(py_system_error, "build_class with non-dictionary");
		return NULL;
	}

	return py_class_new(v, w);
}


/* Status code for main loop (reason for stack unwind) */

enum why_code {
	WHY_NOT,        /* No error */
	WHY_EXCEPTION,  /* Exception occurred */
	WHY_RERAISE,    /* Exception re-raised by 'finally' */
	WHY_RETURN,     /* 'return' statement */
	WHY_BREAK       /* 'break' statement */
};

char* getname(struct py_frame* f, int i) {
	return py_string_get_value(py_list_get(f->code->names, i));
}

/* Interpreter main loop */

struct py_object* py_code_eval(
		struct py_code* co, struct py_object* globals, struct py_object* locals,
		struct py_object* arg) {

	unsigned char* next_instr;
	int opcode; /* Current opcode */
	int oparg; /* Current opcode argument, if any */
	struct py_object** stack_pointer;
	enum why_code why; /* Reason for block stack unwind */
	int err; /* Error status -- nonzero if error */
	struct py_object* x; /* Result object -- NULL if error */
	struct py_object* v; /* Temporary objects popped off stack */
	struct py_object* w;
	struct py_object* u;
	struct py_object* t;
	struct py_frame* f; /* Current frame */
	int lineno; /* Current line number */
	struct py_object* retval; /* Return value iff why == WHY_RETURN */
	char* name; /* Name used by some instructions */

#ifdef TRACE
	int trace = py_dict_lookup(globals, "__trace__") != NULL;
#endif

	/* Code access macros */

#define GETNAME(i) getname(f, i)
#define FIRST_INSTR() (GETUSTRINGVALUE(f->code->code))
#define INSTR_OFFSET() (next_instr - FIRST_INSTR())
#define NEXTOP() (*next_instr++)
#define NEXTARG() (next_instr += 2, (next_instr[-1]<<8) + next_instr[-2])
#define JUMPTO(x) (next_instr = FIRST_INSTR() + (x))
#define JUMPBY(x) (next_instr += (x))

	/* Stack manipulation macros */

/* TODO: These could largely be functions. */

#define STACK_LEVEL() (stack_pointer - f->valuestack)
#define EMPTY() (STACK_LEVEL() == 0)
#define TOP() (stack_pointer[-1])
#define BASIC_PUSH(v) (*stack_pointer++ = (v))
#define BASIC_POP() (*--stack_pointer)

#ifdef TRACE
# define PUSH(v) (BASIC_PUSH(v), prtrace(TOP(), "push", trace))
# define POP() (prtrace(TOP(), "pop", trace), BASIC_POP())
#else
# define PUSH(v) BASIC_PUSH(v)
# define POP() BASIC_POP()
#endif

	f = py_frame_new(
			py_frame_current, /* back */
			co, /* code */
			globals, /* globals */
			locals, /* locals */
			50, /* nvalues */
			20); /* nblocks */
	if(f == NULL) return NULL;

	py_frame_current = f;
	next_instr = GETUSTRINGVALUE(f->code->code);
	stack_pointer = f->valuestack;

	if(arg != NULL) {
		py_object_incref(arg);
		PUSH(arg);
	}

	why = WHY_NOT;
	err = 0;
	x = PY_NONE;       /* Not a reference, just anything non-NULL */
	lineno = -1;

	for(;;) {
		/* Extract opcode and argument */

		opcode = NEXTOP();
		if(opcode >= PY_OP_HAVE_ARGUMENT) oparg = NEXTARG();

#ifdef TRACE
		/* Instruction tracing */

		if(trace) {
			if(opcode >= PY_OP_HAVE_ARGUMENT) {
				printf(
						"%d: %d, %d\n", (int) (INSTR_OFFSET() - 3), opcode,
						oparg);
			}
			else {
				printf("%d: %d\n", (int) (INSTR_OFFSET() - 1), opcode);
			}
		}
#endif

		/* Main switch on opcode */

		switch(opcode) {
			/*
			 * BEWARE!
			 * It is essential that any operation that fails sets either
			 * x to NULL, err to nonzero, or why to anything but WHY_NOT,
			 * and that no operation that succeeds does this!
			 */

			/* case PY_OP_STOP: this is an error! */

			case PY_OP_POP_TOP: {
				v = POP();
				py_object_decref(v);

				break;
			}

			case PY_OP_ROT_TWO: {
				v = POP();
				w = POP();
				PUSH(v);
				PUSH(w);

				break;
			}

			case PY_OP_ROT_THREE: {
				v = POP();
				w = POP();
				x = POP();
				PUSH(v);
				PUSH(x);
				PUSH(w);

				break;
			}

			case PY_OP_DUP_TOP: {
				v = TOP();
				py_object_incref(v);
				PUSH(v);

				break;
			}

			case PY_OP_UNARY_POSITIVE: {
				v = POP();
				x = py_object_pos(v);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_UNARY_NEGATIVE: {
				v = POP();
				x = py_object_neg(v);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_UNARY_NOT: {
				v = POP();
				x = py_object_not(v);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_UNARY_CONVERT: {
				v = POP();
				x = py_object_repr(v);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_UNARY_CALL: {
				v = POP();

				if(py_is_classmethod(v) || py_is_func(v)) {
					x = py_call_function(v, (struct py_object*) NULL);
				}
				else { x = call_builtin(v, (struct py_object*) NULL); }

				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_MULTIPLY: {
				w = POP();
				v = POP();
				x = py_object_mul(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_DIVIDE: {
				w = POP();
				v = POP();
				x = py_object_div(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_MODULO: {
				w = POP();
				v = POP();
				x = py_object_mod(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_ADD: {
				w = POP();
				v = POP();
				x = py_object_add(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_SUBTRACT: {
				w = POP();
				v = POP();
				x = py_object_sub(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_SUBSCR: {
				w = POP();
				v = POP();
				x = py_object_ind(v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_BINARY_CALL: {
				w = POP();
				v = POP();

				if(py_is_classmethod(v) || py_is_func(v)) {
					x = py_call_function(v, w);
				}
				else { x = call_builtin(v, w); }

				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

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
				if((opcode - PY_OP_SLICE) & 2) { w = POP(); }
				else { w = NULL; }

				if((opcode - PY_OP_SLICE) & 1) { v = POP(); }
				else { v = NULL; }

				u = POP();
				x = apply_slice(u, v, w);
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);

				break;
			}

			case PY_OP_STORE_SLICE + 0: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_STORE_SLICE + 1: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_STORE_SLICE + 2: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_STORE_SLICE + 3: {
				if((opcode - PY_OP_STORE_SLICE) & 2) { w = POP(); }
				else { w = NULL; }

				if((opcode - PY_OP_STORE_SLICE) & 1) { v = POP(); }
				else { v = NULL; }

				u = POP();
				t = POP();
				err = assign_slice(u, v, w, t); /* u[v:w] = t */
				py_object_decref(t);
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_DELETE_SLICE + 0: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_DELETE_SLICE + 1: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_DELETE_SLICE + 2: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_DELETE_SLICE + 3: {
				if((opcode - PY_OP_DELETE_SLICE) & 2) { w = POP(); }
				else { w = NULL; }

				if((opcode - PY_OP_DELETE_SLICE) & 1) { v = POP(); }
				else { v = NULL; }

				u = POP();
				err = assign_slice(u, v, w, (struct py_object*) NULL);
				/* del u[v:w] */
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_STORE_SUBSCR: {
				w = POP();
				v = POP();
				u = POP();
				/* v[w] = u */
				err = assign_subscript(v, w, u);
				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_DELETE_SUBSCR: {
				w = POP();
				v = POP();
				/* del v[w] */
				err = assign_subscript(v, w, (struct py_object*) NULL);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			/* TODO: De-printify opcodes. */
			case PY_OP_PRINT_EXPR: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_OP_PRINT_ITEM: {
				v = POP();
				py_object_decref(v);
				break;
			}
			case PY_OP_PRINT_NEWLINE: ;break;

			case PY_OP_BREAK_LOOP: {
				why = WHY_BREAK;

				break;
			}

			case PY_OP_RAISE_EXCEPTION: {
				v = POP();
				w = POP();
				if(!py_is_string(w)) {
					py_error_set_string(
							py_type_error, "exceptions must be strings");
				}
				else { py_error_set_value(w, v); }

				py_object_decref(v);
				py_object_decref(w);
				why = WHY_EXCEPTION;

				break;
			}

			case PY_OP_LOAD_LOCALS: {
				v = f->locals;
				py_object_incref(v);
				PUSH(v);

				break;
			}

			case PY_OP_RETURN_VALUE: {
				retval = POP();
				why = WHY_RETURN;

				break;
			}

			case PY_OP_REQUIRE_ARGS: {
				if(EMPTY()) {
					py_error_set_string(
							py_type_error, "function expects argument(s)");
					why = WHY_EXCEPTION;
				}

				break;
			}

			case PY_OP_REFUSE_ARGS: {
				if(!EMPTY()) {
					py_error_set_string(
							py_type_error, "function expects no argument(s)");
					why = WHY_EXCEPTION;
				}

				break;
			}

			case PY_OP_BUILD_FUNCTION: {
				v = POP();
				x = py_func_new(v, f->globals);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_POP_BLOCK: {
				struct py_block* b = py_block_pop(f);

				while(STACK_LEVEL() > b->level) {
					v = POP();
					py_object_decref(v);
				}

				break;
			}

			case PY_OP_END_FINALLY: {
				v = POP();

				if(py_is_int(v)) {
					why = (enum why_code) py_int_get(v);
					if(why == WHY_RETURN) retval = POP();
				}
				else if(py_is_string(v)) {
					w = POP();
					py_error_set_value(v, w);
					py_object_decref(w);
					w = POP();
					py_traceback_set(w);
					py_object_decref(w);
					why = WHY_RERAISE;
				}
				else if(v != PY_NONE) {
					py_error_set_string(
							py_system_error, "'finally' pops bad exception");
					why = WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_BUILD_CLASS: {
				w = POP();
				v = POP();
				x = build_class(v, w);
				PUSH(x);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_STORE_NAME: {
				name = GETNAME(oparg);
				v = POP();
				err = py_dict_insert(f->locals, name, v);
				py_object_decref(v);

				break;
			}

			case PY_OP_DELETE_NAME: {
				name = GETNAME(oparg);
				if((err = py_dict_remove(f->locals, name)) != 0) {
					py_error_set_string(py_name_error, name);
				}

				break;
			}

			case PY_OP_UNPACK_TUPLE: {
				v = POP();

				if(!py_is_tuple(v)) {
					py_error_set_string(py_type_error, "unpack non-tuple");
					why = WHY_EXCEPTION;
				}
				else if(py_varobject_size(v) != (unsigned) oparg) {
					py_error_set_string(
							py_runtime_error, "unpack tuple of wrong size");
					why = WHY_EXCEPTION;
				}
				else {
					for(; --oparg >= 0;) {
						w = py_tuple_get(v, oparg);
						py_object_incref(w);
						PUSH(w);
					}
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_UNPACK_LIST: {
				v = POP();
				if(!py_is_list(v)) {
					py_error_set_string(py_type_error, "unpack non-list");
					why = WHY_EXCEPTION;
				}
				else if(py_varobject_size(v) != (unsigned) oparg) {
					py_error_set_string(
							py_runtime_error, "unpack list of wrong size");
					why = WHY_EXCEPTION;
				}
				else {
					for(; --oparg >= 0;) {
						w = py_list_get(v, oparg);
						py_object_incref(w);
						PUSH(w);
					}
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_STORE_ATTR: {
				name = GETNAME(oparg);
				v = POP();
				u = POP();
				err = py_object_set_attr(v, name, u); /* v.name = u */
				py_object_decref(v);
				py_object_decref(u);

				break;
			}

			case PY_OP_DELETE_ATTR: {
				name = GETNAME(oparg);
				v = POP();
				err = py_object_set_attr(v, name, (struct py_object*) NULL);
				/* del v.name */
				py_object_decref(v);

				break;
			}

			case PY_OP_LOAD_CONST: {
				x = py_list_get(f->code->consts, oparg);
				py_object_incref(x);
				PUSH(x);

				break;
			}

			case PY_OP_LOAD_NAME: {
				name = GETNAME(oparg);
				x = py_dict_lookup(f->locals, name);
				if(x == NULL) {
					x = py_dict_lookup(f->globals, name);
					if(x == NULL) {
						x = py_builtin_get(name);
					}
				}
				if(x == NULL) {
					py_error_set_string(py_name_error, name);
				}
				else
					py_object_incref(x);

				PUSH(x);

				break;
			}

			case PY_OP_BUILD_TUPLE: {
				x = py_tuple_new(oparg);

				if(x != NULL) {
					for(; --oparg >= 0;) {
						w = POP();
						err = py_tuple_set(x, oparg, w);

						if(err != 0) break;
					}

					PUSH(x);
				}

				break;
			}

			case PY_OP_BUILD_LIST: {
				x = py_list_new(oparg);

				if(x != NULL) {
					for(; --oparg >= 0;) {
						w = POP();
						err = py_list_set(x, oparg, w);

						if(err != 0) break;
					}

					PUSH(x);
				}

				break;
			}

			case PY_OP_BUILD_MAP: {
				x = py_dict_new();
				PUSH(x);

				break;
			}

			case PY_OP_LOAD_ATTR: {
				name = GETNAME(oparg);
				v = POP();
				x = py_object_get_attr(v, name);
				py_object_decref(v);
				PUSH(x);

				break;
			}

			case PY_OP_COMPARE_OP:w = POP();
				v = POP();
				x = cmp_outcome((enum py_cmp_op) oparg, v, w);
				py_object_decref(v);
				py_object_decref(w);
				PUSH(x);
				break;

			case PY_OP_IMPORT_NAME:name = GETNAME(oparg);
				x = py_import_module(name);
				py_object_incref(x);
				PUSH(x);
				break;

			case PY_OP_IMPORT_FROM:name = GETNAME(oparg);
				v = TOP();
				err = import_from(f->locals, v, name);
				break;

			case PY_OP_JUMP_FORWARD:JUMPBY(oparg);
				break;

			case PY_OP_JUMP_IF_FALSE:
				if(!py_object_truthy(TOP()))
					JUMPBY(oparg);
				break;

			case PY_OP_JUMP_IF_TRUE:
				if(py_object_truthy(TOP()))
					JUMPBY(oparg);
				break;

			case PY_OP_JUMP_ABSOLUTE:JUMPTO(oparg);
				break;

			case PY_OP_FOR_LOOP:
				/* for v in s: ...
				On entry: stack contains s, i.
				On exit: stack contains s, i+1, s[i];
				but if loop exhausted:
				s, i are popped, and we jump */
				w = POP(); /* Loop index */
				v = POP(); /* Sequence struct py_object*/
				u = loop_subscript(v, w);
				if(u != NULL) {
					PUSH(v);
					x = py_int_new(py_int_get(w) + 1);
					PUSH(x);
					py_object_decref(w);
					PUSH(u);
				}
				else {
					py_object_decref(v);
					py_object_decref(w);
					/* A NULL can mean "s exhausted"
					but also an error: */
					if(py_error_occurred()) {
						why = WHY_EXCEPTION;
					}
					else
						JUMPBY(oparg);
				}
				break;

			case PY_OP_SETUP_LOOP:
			case PY_OP_SETUP_EXCEPT:
			case PY_OP_SETUP_FINALLY:
				py_block_setup(
						f, opcode, INSTR_OFFSET() + oparg, STACK_LEVEL());
				break;

			case PY_OP_SET_LINENO:
#ifdef TRACE
				if(trace) {
					printf("--- Line %d ---\n", oparg);
				}
#endif
				lineno = oparg;
				break;

			default:
				fprintf(
						stderr, "XXX lineno: %d, opcode: %d\n", lineno, opcode);
				py_error_set_string(
						py_system_error, "py_code_eval: unknown opcode");
				why = WHY_EXCEPTION;
				break;
		} /* switch */


		/* Quickly continue if no error occurred */

		if(why == WHY_NOT) {
			if(err == 0 && x != NULL) {
				continue;
			} /* Normal, fast path */
			why = WHY_EXCEPTION;
			x = PY_NONE;
			err = 0;
		}

#ifndef NDEBUG
		/* Double-check exception status */

		if(why == WHY_EXCEPTION || why == WHY_RERAISE) {
			if(!py_error_occurred()) {
				fprintf(stderr, "XXX ghost error\n");
				py_error_set_string(py_system_error, "ghost error");
				why = WHY_EXCEPTION;
			}
		}
		else {
			if(py_error_occurred()) {
				fprintf(stderr, "XXX undetected error\n");
				why = WHY_EXCEPTION;
			}
		}
#endif

		/* Log traceback info if this is a real exception */

		if(why == WHY_EXCEPTION) {
			int lasti = INSTR_OFFSET() - 1;
			if(opcode >= PY_OP_HAVE_ARGUMENT) {
				lasti -= 2;
			}
			py_traceback_new(f, lasti, lineno);
		}

		/* For the rest, treat WHY_RERAISE as WHY_EXCEPTION */

		if(why == WHY_RERAISE) {
			why = WHY_EXCEPTION;
		}

		/* Unwind stacks if a (pseudo) exception occurred */

		while(why != WHY_NOT && f->iblock > 0) {
			struct py_block* b = py_block_pop(f);
			while(STACK_LEVEL() > b->level) {
				v = POP();
				py_object_decref(v);
			}
			if(b->type == PY_OP_SETUP_LOOP && why == WHY_BREAK) {
				why = WHY_NOT;
				JUMPTO(b->handler);
				break;
			}
			if(b->type == PY_OP_SETUP_FINALLY ||
			   (b->type == PY_OP_SETUP_EXCEPT && why == WHY_EXCEPTION)) {
				if(why == WHY_EXCEPTION) {
					struct py_object* exc, * val;
					py_error_get(&exc, &val);
					if(val == NULL) {
						val = PY_NONE;
						py_object_incref(val);
					}
					v = py_traceback_get();
					/* Make the raw exception data
					available to the handler,
					so a program can emulate the
					Python main loop. Don't do
					this for 'finally'. */
					if(b->type == PY_OP_SETUP_EXCEPT) {
						py_error_clear();
					}
					PUSH(v);
					PUSH(val);
					PUSH(exc);
				}
				else {
					if(why == WHY_RETURN)
						PUSH(retval);
					v = py_int_new((long) why);
					PUSH(v);
				}
				why = WHY_NOT;
				JUMPTO(b->handler);
				break;
			}
		} /* unwind stack */

		/* End the loop if we still have an error (or return) */

		if(why != WHY_NOT) {
			break;
		}

	} /* main loop */

	/* Pop remaining stack entries */

	while(!EMPTY()) {
		v = POP();
		py_object_decref(v);
	}

	/* Restore previous frame and release the current one */

	py_frame_current = f->back;
	py_object_decref(f);

	if(why == WHY_RETURN) {
		return retval;
	}
	else {
		return NULL;
	}
}
