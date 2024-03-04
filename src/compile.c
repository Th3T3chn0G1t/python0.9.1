/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Compile an expression node to intermediate code */

/*
 * XXX TO DO:
 * XXX Compute maximum needed stack sizes while compiling
 * XXX Generate simple jump for break/return outside 'try...finally'
 * XXX Include function name in code (and module names?)
 */

#include <python/std.h>
#include <python/env.h>
#include <python/token.h>
#include <python/graminit.h>
#include <python/opcode.h>
#include <python/compile.h>
#include <python/errors.h>

#include <python/listobject.h>
#include <python/intobject.h>
#include <python/floatobject.h>
#include <python/stringobject.h>

#define PY_CODE_CHUNK (1024)

/* Data structure used internally */
struct py_compiler {
	unsigned len;
	unsigned offset; /* index into code */
	py_byte_t* code;

	struct py_object* consts; /* list of objects */
	struct py_object* names; /* list of strings (names) */

	const char* filename; /* filename of current node */

	unsigned in_function; /* set when compiling a function */
	unsigned nesting; /* counts nested loops */
};

static struct py_code* py_code_new(
		py_byte_t* code, struct py_object* consts,
		struct py_object* names, const char* filename) {

	struct py_code* co;
	unsigned i;

	/* Make sure the list of names contains only strings */
	for(i = py_varobject_size(names) + 1; --i >= 1;) {
		struct py_object* v = py_list_get(names, i - 1);
		if(v == NULL || !py_is_string(v)) {
			py_error_set_badcall();
			return NULL;
		}
	}

	co = py_object_new(&py_code_type);
	if(co != NULL) {
		co->code = code;
		py_object_incref(consts);
		co->consts = consts;
		py_object_incref(names);
		co->names = names;
		if((co->filename = py_string_new(filename)) == NULL) {
			py_object_decref(co);
			co = NULL;
		}
	}

	return co;
}

static void py_compile_node(struct py_compiler*, struct py_node*);

static int py_compiler_new(struct py_compiler* c, const char* filename) {
	c->code = 0;
	c->len = 0;

	if((c->consts = py_list_new(0)) == NULL) return 0;
	if((c->names = py_list_new(0)) == NULL) {
		py_object_decref(c->consts);
		return 0;
	}

	c->offset = 0;
	c->in_function = 0;
	c->nesting = 0;
	c->filename = filename;

	return 1;
}

static void py_compiler_delete(struct py_compiler* c) {
	py_object_decref(c->consts);
	py_object_decref(c->names);
}

static void py_compile_add_byte(struct py_compiler* c, py_byte_t byte) {
	if(c->offset >= c->len) {
		/* TODO: Leaky realloc. */
		c->code = realloc(c->code, c->len + PY_CODE_CHUNK);
		memset(c->code + c->len, 0, PY_CODE_CHUNK);
		c->len += PY_CODE_CHUNK;

		if(!c->code) {
			/* TODO: Better nomem handling */
			fprintf(stderr, "out of memory");
			abort();
		}
	}

	c->code[c->offset++] = byte;
}

static void py_compile_add_int(struct py_compiler* c, unsigned x) {
	py_compile_add_byte(c, x & 0xff);
	py_compile_add_byte(c, x >> 8);
}

static void py_compile_add_op_arg(
		struct py_compiler* c, py_byte_t op, unsigned arg) {

	py_compile_add_byte(c, op);
	py_compile_add_int(c, arg);
}

/* Compile a forward reference for backpatching */
static void py_compile_add_forward_reference(
		struct py_compiler* c, py_byte_t op, unsigned* p_anchor) {

	unsigned anchor;

	py_compile_add_byte(c, op);

	anchor = *p_anchor;
	*p_anchor = c->offset;

	py_compile_add_int(c, anchor == 0 ? 0 : c->offset - anchor);
}

static void py_compile_backpatch(struct py_compiler* c, unsigned anchor) {
	unsigned target = c->offset;
	unsigned prev;
	int dist;

	for(;;) {
		/* Make the JUMP instruction at anchor point to target */
		prev = c->code[anchor] + (c->code[anchor + 1] << 8);

		dist = (int) target - (int) (anchor + 2);

		c->code[anchor] = dist & 0xff;
		c->code[anchor + 1] = dist >> 8;

		if(!prev) break;
		anchor -= prev;
	}
}

/* Handle constants and names uniformly */
static unsigned py_compile_add(struct py_object* list, struct py_object* v) {
	unsigned n = py_varobject_size(list);
	unsigned i;

	for(i = n + 1; --i >= 1;) {
		struct py_object* w = py_list_get(list, i - 1);

		if(py_object_cmp(v, w) == 0) return i - 1;
	}

	/* TODO: Better EH. */
	if(py_list_add(list, v) != 0) py_fatal("error traceback please");

	return n;
}

static unsigned py_compile_add_const(
		struct py_compiler* c, struct py_object* v) {

	return py_compile_add(c->consts, v);
}

static void py_compile_add_op_name(
		struct py_compiler* c, py_byte_t op, struct py_node* n) {

	struct py_object* v;
	unsigned i;
	char* name;

	if(n->type == PY_STAR) name = "*";
	else {
		PY_REQ(n, PY_NAME);
		name = n->str;
	}

	if((v = py_string_new(name)) == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		i = py_compile_add(c->names, v);
		py_object_decref(v);
	}

	py_compile_add_op_arg(c, op, i);
}

static struct py_object* py_compile_parse_number(char* s) {
	char* end = s;
	long x = strtol(s, &end, 0);

	if(*end == '\0') return py_int_new(x);

	if(*end == '.' || *end == 'e' || *end == 'E') {
		return py_float_new(strtod(s, 0));
	}

	py_error_set_string(py_runtime_error, "bad number syntax");
	return NULL;
}

static struct py_object* py_compile_parse_string(const char* s) {
	char* buf = 0;
	unsigned len = 0;
	unsigned i;

	for(i = 1; s[i] != '\''; ++i) {
		/* TODO: Leaky realloc. */
		buf = realloc(buf, ++len);
		if(!buf) return py_error_set_nomem();

		buf[len - 1] = s[i];
		if(s[i] != '\\') continue;

#define _(c, v) case c: buf[len - 1] = v; continue
		switch(s[++i]) {
			default: continue;
			_('\\', '\\');
			_('\'', '\'');
			_('b', '\b');
			_('f', '\014');
			_('t', '\t');
			_('n', '\n');
			_('r', '\r');
			_('v', '\013');
			_('E', '\033');
			_('a', '\007');
		}
#undef _
	}

	return py_string_new_size(buf, len);
}

static void py_compile_list_constructor(
		struct py_compiler* c, struct py_node* n) {

	unsigned len, i;

	if(n->type != PY_GRAMMAR_TEST_LIST) PY_REQ(n, PY_GRAMMAR_EXPRESSION_LIST);

	/* PY_GRAMMAR_EXPRESSION_LIST: PY_GRAMMAR_EXPRESSION (',' PY_GRAMMAR_EXPRESSION)* [',']; likewise for PY_GRAMMAR_TEST_LIST */
	len = (n->count + 1) / 2;

	for(i = 0; i < n->count; i += 2) py_compile_node(c, &n->children[i]);

	py_compile_add_op_arg(c, PY_OP_BUILD_LIST, len);
}

static void py_compile_atom(struct py_compiler* c, struct py_node* n) {
	struct py_node* ch;
	struct py_object* v;
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_ATOM);
	ch = &n->children[0];

	switch(ch->type) {
		case PY_LPAR: {
			if(n->children[1].type == PY_RPAR) {
				py_compile_add_op_arg(c, PY_OP_BUILD_TUPLE, 0);
			}
			else py_compile_node(c, &n->children[1]);

			break;
		}

		case PY_LSQB: {
			if(n->children[1].type == PY_RSQB) {
				py_compile_add_op_arg(c, PY_OP_BUILD_LIST, 0);
			}
			else py_compile_list_constructor(c, &n->children[1]);

			break;
		}

		case PY_LBRACE: {
			py_compile_add_op_arg(c, PY_OP_BUILD_MAP, 0);

			break;
		}

		case PY_NUMBER: {
			if((v = py_compile_parse_number(ch->str)) == NULL) {
				/* TODO: Proper EH. */
				abort();
			}
			else {
				i = py_compile_add_const(c, v);
				py_object_decref(v);
			}

			py_compile_add_op_arg(c, PY_OP_LOAD_CONST, i);

			break;
		}

		case PY_STRING: {
			if((v = py_compile_parse_string(ch->str)) == NULL) {
				/* TODO: Proper EH. */
				abort();
			}
			else {
				i = py_compile_add_const(c, v);
				py_object_decref(v);
			}

			py_compile_add_op_arg(c, PY_OP_LOAD_CONST, i);

			break;
		}

		case PY_NAME: {
			py_compile_add_op_name(c, PY_OP_LOAD_NAME, ch);

			break;
		}

		default: {
			fprintf(stderr, "node type %d\n", ch->type);
			py_error_set_string(
					py_system_error, "py_compile_atom: unexpected node type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void py_compile_apply_subscript(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_SUBSCRIPT);

	if(n->count == 1 && n->children[0].type != PY_COLON) {
		/* It's a single PY_GRAMMAR_SUBSCRIPT */
		py_compile_node(c, &n->children[0]);
		py_compile_add_byte(c, PY_OP_BINARY_SUBSCR);
	}
	else {
		/* It's a slice: [PY_GRAMMAR_EXPRESSION] ':' [PY_GRAMMAR_EXPRESSION] */
		if(n->count == 1) py_compile_add_byte(c, PY_OP_SLICE);
		else if(n->count == 2) {
			if(n->children[0].type != PY_COLON) {
				py_compile_node(c, &n->children[0]);
				py_compile_add_byte(c, PY_OP_SLICE + 1);
			}
			else {
				py_compile_node(c, &n->children[1]);
				py_compile_add_byte(c, PY_OP_SLICE + 2);
			}
		}
		else {
			py_compile_node(c, &n->children[0]);
			py_compile_node(c, &n->children[2]);
			py_compile_add_byte(c, PY_OP_SLICE + 3);
		}
	}
}

static void py_compile_select_member(struct py_compiler* c, struct py_node* n) {
	py_compile_add_op_name(c, PY_OP_LOAD_ATTR, n);
}

static void com_apply_trailer(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_TRAILER);

	switch(n->children[0].type) {
		case PY_LPAR: {
			if(n->children[1].type == PY_RPAR) {
				py_compile_add_byte(c, PY_OP_UNARY_CALL);
			}
			else {
				py_compile_node(c, &n->children[1]);
				py_compile_add_byte(c, PY_OP_BINARY_CALL);
			}

			break;
		}

		case PY_DOT: {
			py_compile_select_member(c, &n->children[1]);

			break;
		}

		case PY_LSQB: {
			py_compile_apply_subscript(c, &n->children[1]);

			break;
		}

		default: {
			py_error_set_string(
					py_system_error, "com_apply_trailer: unknown PY_GRAMMAR_TRAILER type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void py_compile_factor(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_FACTOR);

	if(n->children[0].type == PY_MINUS) {
		py_compile_factor(c, &n->children[1]);
		py_compile_add_byte(c, PY_OP_UNARY_NEGATIVE);
	}
	else {
		py_compile_atom(c, &n->children[0]);
		for(i = 1; i < n->count; i++) com_apply_trailer(c, &n->children[i]);
	}
}

static void py_compile_term(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	unsigned op;

	PY_REQ(n, PY_GRAMMAR_TERM);

	py_compile_factor(c, &n->children[0]);

	for(i = 2; i < n->count; i += 2) {
		py_compile_factor(c, &n->children[i]);

		switch(n->children[i - 1].type) {
			case PY_STAR: {
				op = PY_OP_BINARY_MULTIPLY;

				break;
			}

			case PY_SLASH: {
				op = PY_OP_BINARY_DIVIDE;

				break;
			}

			case PY_PERCENT: {
				op = PY_OP_BINARY_MODULO;

				break;
			}

			default: {
				py_error_set_string(
						py_system_error,
						"py_compile_term: term operator not *, / or %");
				/* TODO: Proper EH. */
				abort();
			}
		}

		py_compile_add_byte(c, op);
	}
}

static void py_compile_expression(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	py_byte_t op;

	PY_REQ(n, PY_GRAMMAR_EXPRESSION);

	py_compile_term(c, &n->children[0]);

	for(i = 2; i < n->count; i += 2) {
		py_compile_term(c, &n->children[i]);

		switch(n->children[i - 1].type) {
			case PY_PLUS: {
				op = PY_OP_BINARY_ADD;

				break;
			}

			case PY_MINUS: {
				op = PY_OP_BINARY_SUBTRACT;

				break;
			}

			default: {
				py_error_set_string(
						py_system_error,
						"py_compile_expression: "
						"expr operator not + or -");
				/* TODO: Proper EH. */
				abort();
			}
		}

		py_compile_add_byte(c, op);
	}
}

static enum py_cmp_op py_compile_compare_type(struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_COMPARE_OP);

	/* PY_GRAMMAR_COMPARE_OP: '<' | '>' | '=' | '>' '=' | '<' '=' | '<' '>'
			  | 'in' | 'not' 'in' | 'is' | 'is' not' */
	if(n->count == 1) {
		n = &n->children[0];

		switch(n->type) {
			case PY_LESS: return PY_CMP_LT;
			case PY_GREATER: return PY_CMP_GT;
			case PY_EQUAL: return PY_CMP_EQ;
			case PY_NAME: {
				if(strcmp(n->str, "in") == 0) return PY_CMP_IN;
				if(strcmp(n->str, "is") == 0) return PY_CMP_IS;
			}
		}
	}
	else if(n->count == 2) {
		int t2 = n->children[1].type;

		switch(n->children[0].type) {
			case PY_LESS: {
				if(t2 == PY_EQUAL) return PY_CMP_LE;
				if(t2 == PY_GREATER) return PY_CMP_NE;

				break;
			}

			case PY_GREATER: {
				if(t2 == PY_EQUAL) return PY_CMP_GE;

				break;
			}

			case PY_NAME: {
				if(strcmp(n->children[1].str, "in") == 0) return PY_CMP_NOT_IN;
				if(strcmp(n->children[0].str, "is") == 0) return PY_CMP_IS_NOT;
			}
		}
	}

	return PY_CMP_BAD;
}

static void py_compile_comparison(struct py_compiler* c, struct py_node* n) {
	enum py_cmp_op op;
	unsigned anchor = 0;
	unsigned i;

	/*
	 * PY_GRAMMAR_TEST_COMPARE:
	 * PY_GRAMMAR_EXPRESSION (PY_GRAMMAR_COMPARE_OP PY_GRAMMAR_EXPRESSION)*
	 */
	PY_REQ(n, PY_GRAMMAR_TEST_COMPARE);

	py_compile_expression(c, &n->children[0]);
	if(n->count == 1) return;

	/*
	 * The following code is generated for all but the last
	 * PY_GRAMMAR_TEST_COMPARE in a chain:
	 *
	 * label:       on stack:       opcode:         jump to:
	 *
	 * 			a               <code to load b>
	 * 			a, b            PY_OP_DUP_TOP
	 * 			a, b, b         PY_OP_ROT_THREE
	 * 			b, a, b         PY_OP_COMPARE_OP
	 * 			b, 0-or-1       PY_OP_JUMP_IF_FALSE   L1
	 * 			b, 1            PY_OP_POP_TOP
	 * 			b
	 *
	 * We are now ready to repeat this sequence for the next
	 * PY_GRAMMAR_TEST_COMPARE in the chain.
	 *
	 * For the last we generate:
	 *
	 * 			b               <code to load c>
	 * 			b, c            PY_OP_COMPARE_OP
	 * 			0-or-1
	 *
	 * If there were any jumps to L1 (i.e., there was more than one
	 * PY_GRAMMAR_TEST_COMPARE), we generate:
	 *
	 * 			0-or-1          PY_OP_JUMP_FORWARD    L2
	 * L1:          b, 0            PY_OP_ROT_TWO
	 * 			0, b            PY_OP_POP_TOP
	 * 			0
	 * L2:
	 */

	for(i = 2; i < n->count; i += 2) {
		py_compile_expression(c, &n->children[i]);

		if(i + 2 < n->count) {
			py_compile_add_byte(c, PY_OP_DUP_TOP);
			py_compile_add_byte(c, PY_OP_ROT_THREE);
		}

		op = py_compile_compare_type(&n->children[i - 1]);
		if(op == PY_CMP_BAD) {
			py_error_set_string(
					py_system_error, "py_compile_comparison: unknown PY_GRAMMAR_TEST_COMPARE op");
			/* TODO: Proper EH. */
			abort();
		}

		py_compile_add_op_arg(c, PY_OP_COMPARE_OP, op);
		if(i + 2 < n->count) {
			py_compile_add_forward_reference(c, PY_OP_JUMP_IF_FALSE, &anchor);
			py_compile_add_byte(c, PY_OP_POP_TOP);
		}
	}

	if(anchor) {
		unsigned anchor2 = 0;

		py_compile_add_forward_reference(c, PY_OP_JUMP_FORWARD, &anchor2);
		py_compile_backpatch(c, anchor);
		py_compile_add_byte(c, PY_OP_ROT_TWO);
		py_compile_add_byte(c, PY_OP_POP_TOP);
		py_compile_backpatch(c, anchor2);
	}
}

static void py_compile_test_not(struct py_compiler* c, struct py_node* n) {
	/* 'not' PY_GRAMMAR_TEST_NOT | PY_GRAMMAR_TEST_COMPARE */
	PY_REQ(n, PY_GRAMMAR_TEST_NOT);

	if(n->count == 1) py_compile_comparison(c, &n->children[0]);
	else {
		py_compile_test_not(c, &n->children[1]);
		py_compile_add_byte(c, PY_OP_UNARY_NOT);
	}
}

static void py_compile_test_and(struct py_compiler* c, struct py_node* n) {
	unsigned i = 0;
	unsigned anchor = 0;

	PY_REQ(n, PY_GRAMMAR_TEST_AND); /* PY_GRAMMAR_TEST_NOT ('and' PY_GRAMMAR_TEST_NOT)* */

	for(;;) {
		py_compile_test_not(c, &n->children[i]);

		if((i += 2) >= n->count) break;

		py_compile_add_forward_reference(c, PY_OP_JUMP_IF_FALSE, &anchor);
		py_compile_add_byte(c, PY_OP_POP_TOP);
	}

	if(anchor) py_compile_backpatch(c, anchor);
}

static void py_compile_test(struct py_compiler* c, struct py_node* n) {
	unsigned i = 0;
	unsigned anchor = 0;

	/* PY_GRAMMAR_TEST_AND ('and' PY_GRAMMAR_TEST_AND)* */
	PY_REQ(n, PY_GRAMMAR_TEST);

	for(;;) {
		py_compile_test_and(c, &n->children[i]);

		if((i += 2) >= n->count) break;

		py_compile_add_forward_reference(c, PY_OP_JUMP_IF_TRUE, &anchor);
		py_compile_add_byte(c, PY_OP_POP_TOP);
	}

	if(anchor) py_compile_backpatch(c, anchor);
}

static void py_compile_list(struct py_compiler* c, struct py_node* n) {
	/*
	 * PY_GRAMMAR_EXPRESSION_LIST:
	 * PY_GRAMMAR_EXPRESSION (',' PY_GRAMMAR_EXPRESSION)* [','];
	 *
	 * likewise for PY_GRAMMAR_TEST_LIST
	 */
	if(n->count == 1) py_compile_node(c, &n->children[0]);
	else {
		unsigned i;
		unsigned len = (n->count + 1) / 2;

		for(i = 0; i < n->count; i += 2) py_compile_node(c, &n->children[i]);

		py_compile_add_op_arg(c, PY_OP_BUILD_TUPLE, len);
	}
}


/* Begin of assignment compilation */

static void py_compile_assign_trailer(
		struct py_compiler* c, struct py_node* n) {

	PY_REQ(n, PY_GRAMMAR_TRAILER);

	switch(n->children[0].type) {
		case PY_LPAR: { /* '(' [PY_GRAMMAR_EXPRESSION_LIST] ')' */
			py_error_set_string(
					py_type_error, "can't assign to function call");
			/* TODO: Proper EH. */
			abort();
		}

		case PY_DOT: { /* '.' PY_NAME */
			py_compile_add_op_name(
					c, c ? PY_OP_STORE_ATTR : PY_OP_DELETE_ATTR,
					&n->children[1]);

			break;
		}

		case PY_LSQB: { /* '[' PY_GRAMMAR_SUBSCRIPT ']' */
			n = &n->children[1];

			PY_REQ(n, PY_GRAMMAR_SUBSCRIPT); /* PY_GRAMMAR_SUBSCRIPT: PY_GRAMMAR_EXPRESSION | [PY_GRAMMAR_EXPRESSION] ':' [PY_GRAMMAR_EXPRESSION] */

			py_compile_node(c, &n->children[0]);
			py_compile_add_byte(c, c ? PY_OP_STORE_SUBSCR : PY_OP_DELETE_SUBSCR);

			break;
		}

		default: {
			py_error_set_string(
					py_type_error, "unknown PY_GRAMMAR_TRAILER type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void py_compile_assign(
		struct py_compiler* c, struct py_node* n);

static void py_compile_assign_tuple(
		struct py_compiler* c, struct py_node* n) {

	unsigned i;

	if(n->type != PY_GRAMMAR_TEST_LIST) PY_REQ(n, PY_GRAMMAR_EXPRESSION_LIST);

	py_compile_add_op_arg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);

	for(i = 0; i < n->count; i += 2) py_compile_assign(c, &n->children[i]);
}

static void py_compile_assign_list(
		struct py_compiler* c, struct py_node* n) {

	unsigned i;
	py_compile_add_op_arg(c, PY_OP_UNPACK_LIST, (n->count + 1) / 2);

	for(i = 0; i < n->count; i += 2) py_compile_assign(c, &n->children[i]);
}

static void py_compile_assign_name(
		struct py_compiler* c, struct py_node* n) {

	PY_REQ(n, PY_NAME);
	py_compile_add_op_name(c, PY_OP_STORE_NAME, n);
}

static void py_compile_assign(
		struct py_compiler* c, struct py_node* n) {

	/* Loop to avoid trivial recursion */
	for(;;) {
		switch(n->type) {
			case PY_GRAMMAR_EXPRESSION_LIST: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_GRAMMAR_TEST_LIST: {
				if(n->count > 1) {
					py_compile_assign_tuple(c, n);
					return;
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_TEST: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_GRAMMAR_TEST_AND: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_GRAMMAR_TEST_NOT: {
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					/* TODO: Proper EH. */
					abort();
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_TEST_COMPARE: {
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					/* TODO: Proper EH. */
					abort();
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_EXPRESSION: {
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					/* TODO: Proper EH. */
					abort();
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_TERM: {
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					/* TODO: Proper EH. */
					abort();
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_FACTOR: {/* ('+'|'-') PY_GRAMMAR_FACTOR | PY_GRAMMAR_ATOM PY_GRAMMAR_TRAILER* */
				if(n->children[0].type != PY_GRAMMAR_ATOM) { /* '+' | '-' */
					py_error_set_string(
							py_type_error, "can't assign to operator");
					/* TODO: Proper EH. */
					abort();
				}

				if(n->count > 1) { /* PY_GRAMMAR_TRAILER present */
					unsigned i;

					py_compile_node(c, &n->children[0]);
					for(i = 1; i + 1 < n->count; i++) {
						com_apply_trailer(c, &n->children[i]);
					} /* NB i is still alive */

					py_compile_assign_trailer(
							c, &n->children[i]);

					return;
				}

				n = &n->children[0];

				break;
			}

			case PY_GRAMMAR_ATOM: {
				switch(n->children[0].type) {
					case PY_LPAR: {
						n = &n->children[1];

						if(n->type == PY_RPAR) {
							/* XXX Should allow () = () ??? */
							py_error_set_string(
									py_type_error, "can't assign to ()");
							/* TODO: Proper EH. */
							abort();
						}

						break;
					}

					case PY_LSQB: {
						n = &n->children[1];

						if(n->type == PY_RSQB) {
							py_error_set_string(
									py_type_error, "can't assign to []");
							/* TODO: Proper EH. */
							abort();
						}

						py_compile_assign_list(c, n);

						return;
					}

					case PY_NAME: {
						py_compile_assign_name(c, &n->children[0]);

						return;
					}

					default: {
						py_error_set_string(
								py_type_error, "can't assign to constant");
						/* TODO: Proper EH. */
						abort();
					}
				}

				break;
			}

			default: {
				fprintf(stderr, "node type %d\n", n->type);
				py_error_set_string(py_system_error, "py_compile_assign: bad node");
				/* TODO: Proper EH. */
				abort();
			}
		}
	}
}

static void py_compile_expression_stmt(
		struct py_compiler* c, struct py_node* n) {

	/*
	 * PY_GRAMMAR_EXPRESSION_LIST
	 * ('=' PY_GRAMMAR_EXPRESSION_LIST)* PY_NEWLINE
	 */
	PY_REQ(n, PY_GRAMMAR_EXPRESSION_STATEMENT);

	py_compile_node(c, &n->children[n->count - 2]);
	if(n->count == 2) py_compile_add_byte(c, PY_OP_PRINT_EXPR);
	else {
		unsigned i;
		for(i = 0; i < n->count - 3; i += 2) {
			if(i + 2 < n->count - 3) py_compile_add_byte(c, PY_OP_DUP_TOP);
			py_compile_assign(c, &n->children[i]);
		}
	}
}

static void py_compile_return_statement(
		struct py_compiler* c, struct py_node* n) {

	/* 'return' [PY_GRAMMAR_TEST_LIST] PY_NEWLINE */
	PY_REQ(n, PY_GRAMMAR_RETURN_STATEMENT);

	if(!c->in_function) {
		py_error_set_string(py_type_error, "'return' outside function");
		/* TODO: Proper EH. */
		abort();
	}

	if(n->count == 2) {
		py_compile_add_op_arg(
				c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));
	}
	else py_compile_node(c, &n->children[1]);

	py_compile_add_byte(c, PY_OP_RETURN_VALUE);
}

static void py_compile_raise_statement(struct py_compiler* c, struct py_node* n) {
	/* 'raise' PY_GRAMMAR_EXPRESSION [',' PY_GRAMMAR_EXPRESSION] PY_NEWLINE */
	PY_REQ(n, PY_GRAMMAR_RAISE_STATEMENT);

	py_compile_node(c, &n->children[1]);

	if(n->count > 3) py_compile_node(c, &n->children[3]);
	else py_compile_add_op_arg(c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));

	py_compile_add_byte(c, PY_OP_RAISE_EXCEPTION);
}

static void py_compile_import_statement(
		struct py_compiler* c, struct py_node* n) {

	unsigned i;

	PY_REQ(n, PY_GRAMMAR_IMPORT_STATEMENT);

	/*
	 * 'import' PY_NAME (',' PY_NAME)* PY_NEWLINE |
	 * 'from' PY_NAME 'import' ('*' | PY_NAME (',' PY_NAME)*) PY_NEWLINE
	 */
	if(n->children[0].str[0] == 'f') {
		/* 'from' PY_NAME 'import' ... */
		PY_REQ(&n->children[1], PY_NAME);

		py_compile_add_op_name(c, PY_OP_IMPORT_NAME, &n->children[1]);

		for(i = 3; i < n->count; i += 2) {
			py_compile_add_op_name(c, PY_OP_IMPORT_FROM, &n->children[i]);
		}

		py_compile_add_byte(c, PY_OP_POP_TOP);
	}
	else {
		/* 'import' ... */
		for(i = 1; i < n->count; i += 2) {
			py_compile_add_op_name(c, PY_OP_IMPORT_NAME, &n->children[i]);
			py_compile_add_op_name(c, PY_OP_STORE_NAME, &n->children[i]);
		}
	}
}

static void py_compile_if_statement(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	unsigned anchor = 0;

	/*
	 * 'if' PY_GRAMMAR_TEST ':'
	 * PY_GRAMMAR_SUITE ('elif' PY_GRAMMAR_TEST ':' PY_GRAMMAR_SUITE)*
	 * ['else' ':' PY_GRAMMAR_SUITE]
	 */
	PY_REQ(n, PY_GRAMMAR_IF_STATEMENT);

	for(i = 0; i + 3 < n->count; i += 4) {
		unsigned a = 0;
		struct py_node* ch = &n->children[i + 1];

		if(i > 0) py_compile_add_op_arg(c, PY_OP_SET_LINENO, ch->lineno);

		py_compile_node(c, &n->children[i + 1]);
		py_compile_add_forward_reference(c, PY_OP_JUMP_IF_FALSE, &a);
		py_compile_add_byte(c, PY_OP_POP_TOP);

		py_compile_node(c, &n->children[i + 3]);
		py_compile_add_forward_reference(c, PY_OP_JUMP_FORWARD, &anchor);
		py_compile_backpatch(c, a);
		py_compile_add_byte(c, PY_OP_POP_TOP);
	}

	if(i + 2 < n->count) py_compile_node(c, &n->children[i + 2]);

	py_compile_backpatch(c, anchor);
}

static void py_compile_while_statement(
		struct py_compiler* c, struct py_node* n) {

	unsigned break_anchor = 0;
	unsigned anchor = 0;
	unsigned begin;

	/*
	 * 'while' PY_GRAMMAR_TEST ':'
	 * PY_GRAMMAR_SUITE ['else' ':' PY_GRAMMAR_SUITE]
	 */
	PY_REQ(n, PY_GRAMMAR_WHILE_STATEMENT);

	py_compile_add_forward_reference(c, PY_OP_SETUP_LOOP, &break_anchor);

	begin = c->offset;

	py_compile_add_op_arg(c, PY_OP_SET_LINENO, n->lineno);
	py_compile_node(c, &n->children[1]);
	py_compile_add_forward_reference(c, PY_OP_JUMP_IF_FALSE, &anchor);
	py_compile_add_byte(c, PY_OP_POP_TOP);

	c->nesting++;
	py_compile_node(c, &n->children[3]);
	c->nesting--;

	py_compile_add_op_arg(c, PY_OP_JUMP_ABSOLUTE, begin);
	py_compile_backpatch(c, anchor);
	py_compile_add_byte(c, PY_OP_POP_TOP);
	py_compile_add_byte(c, PY_OP_POP_BLOCK);

	if(n->count > 4) py_compile_node(c, &n->children[6]);

	py_compile_backpatch(c, break_anchor);
}

static void py_compile_for_statement(
		struct py_compiler* c, struct py_node* n) {

	struct py_object* v;
	unsigned break_anchor = 0;
	unsigned anchor = 0;
	unsigned begin;

	PY_REQ(n, PY_GRAMMAR_FOR_STATEMENT);

	/*
	 * 'for' PY_GRAMMAR_EXPRESSION_LIST 'in' PY_GRAMMAR_EXPRESSION_LIST ':'
	 * PY_GRAMMAR_SUITE ['else' ':' PY_GRAMMAR_SUITE]
	 */
	py_compile_add_forward_reference(c, PY_OP_SETUP_LOOP, &break_anchor);
	py_compile_node(c, &n->children[3]);

	v = py_int_new(0);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}

	py_compile_add_op_arg(c, PY_OP_LOAD_CONST, py_compile_add_const(c, v));
	py_object_decref(v);

	begin = c->offset;

	py_compile_add_op_arg(c, PY_OP_SET_LINENO, n->lineno);
	py_compile_add_forward_reference(c, PY_OP_FOR_LOOP, &anchor);
	py_compile_assign(c, &n->children[1]);

	c->nesting++;
	py_compile_node(c, &n->children[5]);
	c->nesting--;

	py_compile_add_op_arg(c, PY_OP_JUMP_ABSOLUTE, begin);
	py_compile_backpatch(c, anchor);
	py_compile_add_byte(c, PY_OP_POP_BLOCK);

	if(n->count > 8) py_compile_node(c, &n->children[8]);

	py_compile_backpatch(c, break_anchor);
}

/* Although 'execpt' and 'finally' clauses can be combined
 * syntactically, they are compiled separately. In fact,
 *    try: S
 *    except E1: S1
 *    except E2: S2
 *    ...
 *    finally: Sf
 * is equivalent to
 *    try:
 * 	   try: S
 * 	   except E1: S1
 * 	   except E2: S2
 * 	   ...
 *    finally: Sf
 * meaning that the 'finally' clause is entered even if things
 * go wrong again in an exception handler. Note that this is
 * not the case for exception handlers: at most one is entered.
 *
 * Code generated for "try: S finally: Sf" is as follows:
 *
 * 		   PY_OP_SETUP_FINALLY   L
 * 		   <code for S>
 * 		   PY_OP_POP_BLOCK
 * 		   PY_OP_LOAD_CONST      <nil>
 *    L:      <code for Sf>
 * 		   PY_OP_END_FINALLY
 *
 * The special instructions use the block stack. Each block
 * stack entry contains the instruction that created it (here
 * PY_OP_SETUP_FINALLY), the level of the value stack at the time the
 * block stack entry was created, and a label (here L).
 *
 * PY_OP_SETUP_FINALLY:
 *    Pushes the current value stack level and the label
 *    onto the block stack.
 * PY_OP_POP_BLOCK:
 *    Pops en entry from the block stack, and pops the value
 *    stack until its level is the same as indicated on the
 *    block stack. (The label is ignored.)
 * PY_OP_END_FINALLY:
 *    Pops a variable number of entries from the *value* stack
 *    and re-raises the exception they specify. The number of
 *    entries popped depends on the (pseudo) exception type.
 *
 * The block stack is unwound when an exception is raised:
 * when a PY_OP_SETUP_FINALLY entry is found, the exception is pushed
 * onto the value stack (and the exception condition is cleared),
 * and the interpreter jumps to the label gotten from the block
 * stack.
 *
 * Code generated for "try: S except E1, V1: S1 except E2, V2: S2 ...":
 * (The contents of the value stack is shown in [], with the top
 * at the right; 'tb' is trace-back info, 'val' the exception's
 * associated value, and 'exc' the exception.)
 *
 * Value stack         Label   Instruction     Argument
 * []                          PY_OP_SETUP_EXCEPT    L1
 * []                          <code for S>
 * []                          PY_OP_POP_BLOCK
 * []                          PY_OP_JUMP_FORWARD    L0
 *
 * [tb, val, exc]      L1:     DUP                             )
 * [tb, val, exc, exc]         <evaluate E1>                     )
 * [tb, val, exc, exc, E1]     PY_OP_COMPARE_OP    PY_CMP_EXC_MATCH) only if E1
 * [tb, val, exc, 1-or-0]      PY_OP_JUMP_IF_FALSE   L2              )
 * [tb, val, exc, 1]           POP                             )
 * [tb, val, exc]              POP
 * [tb, val]                   <assign to V1>    (or POP if no V1)
 * [tb]                                POP
 * []                          <code for S1>
 * 						   PY_OP_JUMP_FORWARD    L0
 *
 * [tb, val, exc, 0]   L2:     POP
 * [tb, val, exc]              DUP
 * .............................etc.......................
 *
 * [tb, val, exc, 0]   Ln+1:   POP
 * [tb, val, exc]              PY_OP_END_FINALLY     # re-raise exception
 *
 * []                  L0:     <next statement>
 *
 * Of course, parts are not generated if Vi or Ei is not present.
 */

static void py_compile_try_statement(
		struct py_compiler* c, struct py_node* n) {

	unsigned finally_anchor = 0;
	unsigned except_anchor = 0;

	/*
	 * 'try' ':' PY_GRAMMAR_SUITE
	 * (PY_GRAMMAR_EXCEPT_CLAUSE ':' PY_GRAMMAR_SUITE)*
	 * ['finally' ':' PY_GRAMMAR_SUITE]
	 */
	PY_REQ(n, PY_GRAMMAR_TRY_STATEMENT);

	if(n->count > 3 &&
		n->children[n->count - 3].type != PY_GRAMMAR_EXCEPT_CLAUSE) {

		/* Have a 'finally' clause */
		py_compile_add_forward_reference(
				c, PY_OP_SETUP_FINALLY, &finally_anchor);
	}

	if(n->count > 3 &&
		n->children[3].type == PY_GRAMMAR_EXCEPT_CLAUSE) {

		/* Have an 'except' clause */
		py_compile_add_forward_reference(
				c, PY_OP_SETUP_EXCEPT, &except_anchor);
	}

	py_compile_node(c, &n->children[2]);
	if(except_anchor) {
		unsigned end_anchor = 0;
		unsigned i;
		struct py_node* ch;

		py_compile_add_byte(c, PY_OP_POP_BLOCK);
		py_compile_add_forward_reference(c, PY_OP_JUMP_FORWARD, &end_anchor);
		py_compile_backpatch(c, except_anchor);

		for(i = 3;
				i < n->count &&
				(ch = &n->children[i])->type == PY_GRAMMAR_EXCEPT_CLAUSE;
				i += 3) {

			/*
			 * PY_GRAMMAR_EXCEPT_CLAUSE:
			 * 'except' [PY_GRAMMAR_EXPRESSION [',' PY_GRAMMAR_EXPRESSION]]
			 */
			if(except_anchor == 0) {
				py_error_set_string(
						py_type_error, "default 'except:' must be last");
				/* TODO: Proper EH. */
				abort();
			}

			except_anchor = 0;
			py_compile_add_op_arg(c, PY_OP_SET_LINENO, ch->lineno);

			if(ch->count > 1) {
				py_compile_add_byte(c, PY_OP_DUP_TOP);
				py_compile_node(c, &ch->children[1]);
				py_compile_add_op_arg(c, PY_OP_COMPARE_OP, PY_CMP_EXC_MATCH);
				py_compile_add_forward_reference(c, PY_OP_JUMP_IF_FALSE, &except_anchor);
				py_compile_add_byte(c, PY_OP_POP_TOP);
			}

			py_compile_add_byte(c, PY_OP_POP_TOP);

			if(ch->count > 3) py_compile_assign(c, &ch->children[3]);
			else py_compile_add_byte(c, PY_OP_POP_TOP);

			py_compile_add_byte(c, PY_OP_POP_TOP);
			py_compile_node(c, &n->children[i + 2]);
			py_compile_add_forward_reference(c, PY_OP_JUMP_FORWARD, &end_anchor);

			if(except_anchor) {
				py_compile_backpatch(c, except_anchor);
				py_compile_add_byte(c, PY_OP_POP_TOP);
			}
		}

		py_compile_add_byte(c, PY_OP_END_FINALLY);
		py_compile_backpatch(c, end_anchor);
	}

	if(finally_anchor) {
		struct py_node* ch;

		py_compile_add_byte(c, PY_OP_POP_BLOCK);
		py_compile_add_op_arg(c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));
		py_compile_backpatch(c, finally_anchor);

		ch = &n->children[n->count - 1];

		py_compile_add_op_arg(c, PY_OP_SET_LINENO, ch->lineno);
		py_compile_node(c, ch);
		py_compile_add_byte(c, PY_OP_END_FINALLY);
	}
}

static void py_compile_suite(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_SUITE);

	/*
	 * PY_GRAMMAR_SIMPLE_STATEMENT |
	 * PY_NEWLINE PY_INDENT PY_NEWLINE*
	 * (PY_GRAMMAR_STATEMENT PY_NEWLINE*)+ PY_DEDENT
	 */
	if(n->count == 1) py_compile_node(c, &n->children[0]);
	else {
		unsigned i;
		for(i = 0; i < n->count; i++) {
			struct py_node* ch = &n->children[i];

			if(ch->type == PY_GRAMMAR_STATEMENT) py_compile_node(c, ch);
		}
	}
}

static void py_compile_function_definition(
		struct py_compiler* c, struct py_node* n) {

	struct py_object* v;

	/*
	 * PY_GRAMMAR_FUNCTION_DEFINITION:
	 * 'def' PY_NAME parameters ':' PY_GRAMMAR_SUITE
	 */
	PY_REQ(n, PY_GRAMMAR_FUNCTION_DEFINITION);

	v = (struct py_object*) py_compile(n, c->filename);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		py_compile_add_op_arg(c, PY_OP_LOAD_CONST, py_compile_add_const(c, v));
		py_compile_add_byte(c, PY_OP_BUILD_FUNCTION);
		py_compile_add_op_name(c, PY_OP_STORE_NAME, &n->children[1]);
		py_object_decref(v);
	}
}

static void py_compile_class_definition(
		struct py_compiler* c, struct py_node* n) {

	struct py_object* v;

	/*
	 * PY_GRAMMAR_CLASS_DEFINITION:
	 * 'class' PY_NAME parameters
	 * ['=' PY_GRAMMAR_BASE_LIST] ':' PY_GRAMMAR_SUITE
	 * PY_GRAMMAR_BASE_LIST:
	 * PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS
	 * (',' PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS)*
	 * PY_GRAMMAR_ARGUMENTS:
	 * '(' [PY_GRAMMAR_TEST_LIST] ')'
     */
	PY_REQ(n, PY_GRAMMAR_CLASS_DEFINITION);

	py_compile_add_op_arg(
			c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));

	v = (struct py_object*) py_compile(n, c->filename);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		int i = py_compile_add_const(c, v);
		py_compile_add_op_arg(c, PY_OP_LOAD_CONST, i);
		py_compile_add_byte(c, PY_OP_BUILD_FUNCTION);
		py_compile_add_byte(c, PY_OP_UNARY_CALL);
		py_compile_add_byte(c, PY_OP_BUILD_CLASS);
		py_compile_add_op_name(c, PY_OP_STORE_NAME, &n->children[1]);
		py_object_decref(v);
	}
}

static void py_compile_node(struct py_compiler* c, struct py_node* n) {
	switch(n->type) {
		/* Definition nodes */

		case PY_GRAMMAR_FUNCTION_DEFINITION: {
			py_compile_function_definition(c, n);

			break;
		}

		case PY_GRAMMAR_CLASS_DEFINITION: {
			py_compile_class_definition(c, n);

			break;
		}

		/* Trivial parse tree nodes */

		case PY_GRAMMAR_STATEMENT: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_GRAMMAR_FLOW_STATEMENT: {
			py_compile_node(c, &n->children[0]);
			break;
		}

		case PY_GRAMMAR_SIMPLE_STATEMENT: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_GRAMMAR_COMPOUND_STATEMENT: {
			py_compile_add_op_arg(c, PY_OP_SET_LINENO, n->lineno);
			py_compile_node(c, &n->children[0]);

			break;
		}

		/* Statement nodes */

		case PY_GRAMMAR_EXPRESSION_STATEMENT: {
			py_compile_expression_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_BREAK_STATEMENT: {
			if(c->nesting == 0) {
				py_error_set_string(py_type_error, "'break' outside loop");
				/* TODO: Proper EH. */
				abort();
			}

			py_compile_add_byte(c, PY_OP_BREAK_LOOP);

			break;
		}

		case PY_GRAMMAR_RETURN_STATEMENT: {
			py_compile_return_statement(c, n);

			break;
		}

		case PY_GRAMMAR_RAISE_STATEMENT: {
			py_compile_raise_statement(c, n);

			break;
		}

		case PY_GRAMMAR_IMPORT_STATEMENT: {
			py_compile_import_statement(c, n);

			break;
		}

		case PY_GRAMMAR_IF_STATEMENT: {
			py_compile_if_statement(c, n);

			break;
		}

		case PY_GRAMMAR_WHILE_STATEMENT: {
			py_compile_while_statement(c, n);

			break;
		}

		case PY_GRAMMAR_FOR_STATEMENT: {
			py_compile_for_statement(c, n);

			break;
		}

		case PY_GRAMMAR_TRY_STATEMENT: {
			py_compile_try_statement(c, n);

			break;
		}

		case PY_GRAMMAR_SUITE: {
			py_compile_suite(c, n);

			break;
		}

		/* Expression nodes */

		case PY_GRAMMAR_TEST_LIST: {
			py_compile_list(c, n);

			break;
		}

		case PY_GRAMMAR_TEST: {
			py_compile_test(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_AND: {
			py_compile_test_and(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_NOT: {
			py_compile_test_not(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_COMPARE: {
			py_compile_comparison(c, n);

			break;
		}

		case PY_GRAMMAR_EXPRESSION_LIST: {
			py_compile_list(c, n);

			break;
		}

		case PY_GRAMMAR_EXPRESSION: {
			py_compile_expression(c, n);

			break;
		}

		case PY_GRAMMAR_TERM: {
			py_compile_term(c, n);

			break;
		}

		case PY_GRAMMAR_FACTOR: {
			py_compile_factor(c, n);

			break;
		}

		case PY_GRAMMAR_ATOM: {
			py_compile_atom(c, n);

			break;
		}

		default: {
			fprintf(stderr, "node type %d\n", n->type);
			py_error_set_string(
					py_system_error, "py_compile_node: unexpected node type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void py_compile_parameter_list(struct py_compiler*, struct py_node*);

static void py_compile_parameter_definition(
		struct py_compiler* c, struct py_node* n) {

	/*
	 * PY_GRAMMAR_PARAMETER_DEFINITION:
	 * PY_NAME | '(' PY_GRAMMAR_PARAMETER_LIST ')'
	 */
	PY_REQ(n, PY_GRAMMAR_PARAMETER_DEFINITION);

	if(n->children[0].type == PY_LPAR) {
		py_compile_parameter_list(c, &n->children[1]);
	}
	else py_compile_add_op_name(c, PY_OP_STORE_NAME, &n->children[0]);
}

static void py_compile_parameter_list(
		struct py_compiler* c, struct py_node* n) {

	/*
	 * PY_GRAMMAR_PARAMETER_LIST:
	 * PY_GRAMMAR_PARAMETER_DEFINITION (',' PY_GRAMMAR_PARAMETER_DEFINITION)*
	 */
	PY_REQ(n, PY_GRAMMAR_PARAMETER_LIST);

	if(n->count == 1) py_compile_parameter_definition(c, &n->children[0]);
	else {
		unsigned i;

		py_compile_add_op_arg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);
		for(i = 0; i < n->count; i += 2) {
			py_compile_parameter_definition(c, &n->children[i]);
		}
	}
}

static void py_compile_file_input(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	/* (PY_NEWLINE | PY_GRAMMAR_STATEMENT)* PY_ENDMARKER */
	PY_REQ(n, PY_GRAMMAR_FILE_INPUT);

	for(i = 0; i < n->count; i++) {
		struct py_node* ch = &n->children[i];

		if(ch->type != PY_ENDMARKER && ch->type != PY_NEWLINE) {
			py_compile_node(c, ch);
		}
	}
}

/* Top-level py_compile-node interface */

static void py_compile_function_signature(
		struct py_compiler* c, struct py_node* n) {

	struct py_node* ch;

	/*
	 * PY_GRAMMAR_FUNCTION_DEFINITION:
	 * 'def' PY_NAME parameters ':' PY_GRAMMAR_SUITE
	 */
	PY_REQ(n, PY_GRAMMAR_FUNCTION_DEFINITION);

	ch = &n->children[2]; /* parameters: '(' [PY_GRAMMAR_PARAMETER_LIST] ')' */
	ch = &ch->children[1]; /* ')' | PY_GRAMMAR_PARAMETER_LIST */

	if(ch->type == PY_RPAR) py_compile_add_byte(c, PY_OP_REFUSE_ARGS);
	else {
		py_compile_add_byte(c, PY_OP_REQUIRE_ARGS);
		py_compile_parameter_list(c, ch);
	}

	c->in_function = 1;
	py_compile_node(c, &n->children[4]);
	c->in_function = 0;

	py_compile_add_op_arg(
			c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));
	py_compile_add_byte(c, PY_OP_RETURN_VALUE);
}

static void compile_node(struct py_compiler* c, struct py_node* n) {
	py_compile_add_op_arg(c, PY_OP_SET_LINENO, n->lineno);

	switch(n->type) {
		/* A whole file. */
		case PY_GRAMMAR_FILE_INPUT: {
			py_compile_add_byte(c, PY_OP_REFUSE_ARGS);
			py_compile_file_input(c, n);
			py_compile_add_op_arg(
					c, PY_OP_LOAD_CONST, py_compile_add_const(c, PY_NONE));
			py_compile_add_byte(c, PY_OP_RETURN_VALUE);

			break;
		}

		/* A function definition */
		case PY_GRAMMAR_FUNCTION_DEFINITION: {
			py_compile_function_signature(c, n);

			break;
		}

		case PY_GRAMMAR_CLASS_DEFINITION: { /* A class definition */
			/*
			 * 'class' PY_NAME parameters
			 * ['=' PY_GRAMMAR_BASE_LIST] ':' PY_GRAMMAR_SUITE
			 */
			py_compile_add_byte(c, PY_OP_REFUSE_ARGS);
			py_compile_node(c, &n->children[n->count - 1]);
			py_compile_add_byte(c, PY_OP_LOAD_LOCALS);
			py_compile_add_byte(c, PY_OP_RETURN_VALUE);

			break;
		}

		default: {
			/* TODO: Better EH. */
			fprintf(stderr, "node type %d\n", n->type);
			py_error_set_string(
					py_system_error, "compile_node: unexpected node type");
			abort();
		}
	}
}

struct py_code* py_compile(struct py_node* n, const char* filename) {
	struct py_compiler sc;
	struct py_code* co;

	if(!py_compiler_new(&sc, filename)) return NULL;

	compile_node(&sc, n);

	/* TODO: Leaky realloc. */
	sc.code = realloc(sc.code, sc.offset);
	sc.len = sc.offset;

	co = py_code_new(sc.code, sc.consts, sc.names, filename);

	py_compiler_delete(&sc);
	return co;
}

static void code_dealloc(struct py_object* op) {
	struct py_code* co = (struct py_code*) op;

	free(co->code);
	py_object_decref(co->consts);
	py_object_decref(co->names);
	py_object_decref(co->filename);
}

struct py_type py_code_type = {
		{ 1, &py_type_type }, sizeof(struct py_code),
		code_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};
