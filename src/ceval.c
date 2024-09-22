/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Execute compiled code */

#include <python/state.h>
#include <python/std.h>
#include <python/env.h>
#include <python/evalops.h>
#include <python/import.h>
#include <python/traceback.h>
#include <python/compile.h>
#include <python/ceval.h>
#include <python/errors.h>

#include <python/module/builtin.h>

#include <python/object/frame.h>
#include <python/object/int.h>
#include <python/object/dict.h>
#include <python/object/string.h>
#include <python/object/list.h>
#include <python/object/tuple.h>
#include <python/object/class.h>
#include <python/object/func.h>

#include <apro.h>

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
	int err; /* Error status -- nonzero if error */

	apro_stamp_start(APRO_CEVAL_CODE_EVAL_RISING);

	/* TODO: Why are these constants the random defaults. */
	if(!(f = py_frame_new(env->current, co, globals, locals, 50, 20))) {
		py_error_set_nomem();
		return 0;
	}

	env->current = f;
	code = f->code->code;
	next = code;
	stack_pointer = f->valuestack;

	if(py_object_incref(args)) *stack_pointer++ = args;

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
				py_object_decref(*--stack_pointer);
				break;
			}

			case PY_OP_ROT_TWO: {
				v = *--stack_pointer;
				w = *--stack_pointer;

				*stack_pointer++ = v;
				*stack_pointer++ = w;

				break;
			}

			case PY_OP_ROT_THREE: {
				v = *--stack_pointer;
				w = *--stack_pointer;
				x = *--stack_pointer;

				*stack_pointer++ = v;
				*stack_pointer++ = x;
				*stack_pointer++ = w;

				break;
			}

			case PY_OP_DUP_TOP: {
				v = py_object_incref(stack_pointer[-1]);

				*stack_pointer++ = v;

				break;
			}

			case PY_OP_UNARY_NEGATIVE: {
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_neg(v))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_UNARY_NOT: {
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_not(v))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_BUILD_CLASS: {
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_class_new(v))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_UNARY_CALL: {
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_call_function(env, v, 0))) {
					if(!py_error_occurred()) py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_BINARY_MULTIPLY: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_mul(v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_DIVIDE: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_div(v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_MODULO: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_mod(v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_ADD: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_add(v, w))) {
					py_error_set_badcall();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_SUBTRACT: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_sub(v, w))) {
					py_error_set_badcall();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_SUBSCR: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_object_ind(v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_BINARY_CALL: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_call_function(env, v, w))) {
					if(!py_error_occurred()) py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_SLICE + 0:; PY_FALLTHROUGH;
			/* FALLTHROUGH */
			case PY_OP_SLICE + 1:; PY_FALLTHROUGH;
			/* FALLTHROUGH */
			case PY_OP_SLICE + 2:; PY_FALLTHROUGH;
			/* FALLTHROUGH */
			case PY_OP_SLICE + 3: {
				if((opcode - PY_OP_SLICE) & 2) w = *--stack_pointer;
				else w = 0;

				if((opcode - PY_OP_SLICE) & 1) v = *--stack_pointer;
				else v = 0;

				u = *--stack_pointer;

				if(!(*stack_pointer++ = py_apply_slice(u, v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_STORE_SUBSCR: {
				w = *--stack_pointer;
				v = *--stack_pointer;
				u = *--stack_pointer;

				if(py_assign_subscript(v, w, u) == -1) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(u);
				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			/* TODO: De-printify opcodes. */
			case PY_OP_PRINT_EXPR: {
				py_object_decref(*--stack_pointer);
				break;
			}

			case PY_OP_BREAK_LOOP: {
				why = PY_WHY_BREAK;
				break;
			}

			case PY_OP_LOAD_LOCALS: {
				*stack_pointer++ = py_object_incref(f->locals);
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

				*stack_pointer++ = py_func_new(v, f->globals);

				py_object_decref(v);

				break;
			}

			case PY_OP_POP_BLOCK: {
				struct py_block* b;

				if(!f->iblock) {
					py_error_set_string(py_runtime_error, "stack underflow");
					why = PY_WHY_EXCEPTION;
					break;
				}

				b = py_block_pop(f);

				while((stack_pointer - f->valuestack) > (int) b->level) {
					py_object_decref(*--stack_pointer);
				}

				break;
			}

			case PY_OP_STORE_NAME: {
				v = *--stack_pointer;

				err = py_dict_insert(f->locals, py_code_get_name(f, oparg), v);
				if(err == -1) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_UNPACK_TUPLE: {
				v = *--stack_pointer;

				if(v->type != PY_TYPE_TUPLE) {
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
						w = py_object_incref(py_tuple_get(v, oparg));
						*stack_pointer++ = w;
					}
				}

				py_object_decref(v);

				break;
			}

			/* TODO: Consolidate list/tuple since they do the same thing? */
			case PY_OP_UNPACK_LIST: {
				v = *--stack_pointer;

				if(v->type != PY_TYPE_LIST) {
					py_error_set_string(py_type_error, "unpack non-list");
					why = PY_WHY_EXCEPTION;
					break;
				}

				if(py_varobject_size(v) != (unsigned) oparg) {
					py_error_set_string(
							py_runtime_error, "unpack list of wrong size");

					why = PY_WHY_EXCEPTION;
					break;
				}

				for(; --oparg >= 0;) {
					*stack_pointer++ = py_object_incref(py_list_get(v, oparg));
				}

				py_object_decref(v);

				break;
			}

			case PY_OP_STORE_ATTR: {
				v = *--stack_pointer;
				u = *--stack_pointer;

				err = py_object_set_attr(v, py_code_get_name(f, oparg), u);
				if(err == -1) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(u);

				break;
			}

			case PY_OP_LOAD_CONST: {
				x = py_object_incref(py_list_get(f->code->consts, oparg));
				*stack_pointer++ = x;
				break;
			}

			case PY_OP_LOAD_NAME: {
				const char* name = py_code_get_name(f, oparg);

				if(!(x = py_dict_lookup(f->locals, name))) {
					if(!(x = py_dict_lookup(f->globals, name))) {
						x = py_builtin_get(name);
					}
				}

				if(!x) py_error_set_string(py_name_error, name);

				*stack_pointer++ = py_object_incref(x);

				break;
			}

			case PY_OP_BUILD_TUPLE: {
				if(!(x = py_tuple_new(oparg))) {
					py_error_set_nomem();
					why = PY_WHY_EXCEPTION;
					break;
				}

				for(; --oparg >= 0;) py_tuple_set(x, oparg, *--stack_pointer);

				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BUILD_LIST: {
				if(!(x = py_list_new(oparg))) {
					py_error_set_nomem();
					why = PY_WHY_EXCEPTION;
					break;
				}

				for(; --oparg >= 0;) py_list_set(x, oparg, *--stack_pointer);

				*stack_pointer++ = x;

				break;
			}

			case PY_OP_BUILD_MAP: {
				if(!(*stack_pointer++ = py_dict_new())) {
					py_error_set_nomem();
					why = PY_WHY_EXCEPTION;
				}

				break;
			}

			case PY_OP_LOAD_ATTR: {
				v = *--stack_pointer;

				x = py_object_get_attr(v, py_code_get_name(f, oparg));
				*stack_pointer++ = x;

				py_object_decref(v);

				break;
			}

			case PY_OP_COMPARE_OP: {
				w = *--stack_pointer;
				v = *--stack_pointer;

				if(!(*stack_pointer++ = py_cmp_outcome(oparg, v, w))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				py_object_decref(v);
				py_object_decref(w);

				break;
			}

			case PY_OP_IMPORT_NAME: {
				if(!(v = py_import_module(env, py_code_get_name(f, oparg)))) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

				*stack_pointer++ = py_object_incref(v);

				break;
			}

			case PY_OP_IMPORT_FROM: {
				v = stack_pointer[-1];

				err = py_import_from(f->locals, v, py_code_get_name(f, oparg));
				if(err == -1) {
					py_error_set_evalop();
					why = PY_WHY_EXCEPTION;
				}

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

			case PY_OP_FOR_LOOP: {
				/*
				 * for v in s: ...
				 * On entry: stack contains s, i.
				 * On exit: stack contains s, i+1, s[i];
				 * but if loop exhausted:
				 * s, i are popped, and we jump
				 */

				w = *--stack_pointer; /* Loop index */
				v = *--stack_pointer; /* Sequence struct py_object*/

				if (!py_is_varobject(v)) {
					py_error_set_string(
							py_type_error, "loop over non-sequence");

					why = PY_WHY_EXCEPTION;
					break;
				}

				if (!(u = py_loop_subscript(v, w))) {
					py_object_decref(v);
					py_object_decref(w);

					next += oparg;
					break;
				}

				if (!(x = py_int_new(py_int_get(w) + 1))) {
					py_error_set_nomem();
					why = PY_WHY_EXCEPTION;
				}

				*stack_pointer++ = v;
				*stack_pointer++ = x;
				py_object_decref(w);
				*stack_pointer++ = u;

				break;
			}

			case PY_OP_SETUP_LOOP:; PY_FALLTHROUGH;
			/* FALLTHROUGH */
			case PY_OP_SETUP_EXCEPT: {
				if(f->iblock >= f->nblocks) {
					py_error_set_string(py_runtime_error, "stack overflow");
					why = PY_WHY_EXCEPTION;
					break;
				}

				py_block_setup(
						f, opcode, (next - code) + oparg,
						stack_pointer - f->valuestack);

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
		}

		/* Quickly continue if no error occurred */
		if(why == PY_WHY_NOT) continue; /* Normal, fast path */

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
				py_object_decref(*--stack_pointer);
			}

			if(b->type == PY_OP_SETUP_LOOP && why == PY_WHY_BREAK) {
				why = PY_WHY_NOT;
				next = code + b->handler;
				break;
			}
		}

		/* End the loop if we still have an error (or return) */
		if(why != PY_WHY_NOT) break;
	}

	apro_stamp_end(APRO_CEVAL_CODE_EVAL);

	apro_stamp_start(APRO_CEVAL_CODE_EVAL_FALLING);

	/* Pop remaining stack entries */
	while(stack_pointer - f->valuestack) py_object_decref(*--stack_pointer);

	/* Restore previous frame and release the current one */
	env->current = f->back;
	py_object_decref(f);

	apro_stamp_end(APRO_CEVAL_CODE_EVAL_FALLING);

	return why == PY_WHY_RETURN ? retval : 0;
}
