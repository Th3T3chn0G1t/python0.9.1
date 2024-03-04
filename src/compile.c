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

static void com_node(struct py_compiler*, struct py_node*);

static int com_init(struct py_compiler* c, const char* filename) {
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

static void com_free(struct py_compiler* c) {
	py_object_decref(c->consts);
	py_object_decref(c->names);
}

static void com_done(struct py_compiler* c) {
	/* TODO: Leaky realloc. */
	c->code = realloc(c->code, c->offset);
	c->len = c->offset;
}

static void com_addbyte(struct py_compiler* c, py_byte_t byte) {
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

static void com_addint(struct py_compiler* c, unsigned x) {
	com_addbyte(c, x & 0xff);
	com_addbyte(c, x >> 8); /* XXX x should be positive */
}

static void com_addoparg(struct py_compiler* c, py_byte_t op, unsigned arg) {
	com_addbyte(c, op);
	com_addint(c, arg);
}

/* Compile a forward reference for backpatching */
static void com_addfwref(
		struct py_compiler* c, py_byte_t op, unsigned* p_anchor) {

	unsigned anchor;

	com_addbyte(c, op);

	anchor = *p_anchor;
	*p_anchor = c->offset;

	com_addint(c, anchor == 0 ? 0 : c->offset - anchor);
}

static void com_backpatch(struct py_compiler* c, unsigned anchor) {
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
static unsigned com_add(struct py_object* list, struct py_object* v) {
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

static unsigned com_addconst(struct py_compiler* c, struct py_object* v) {
	return com_add(c->consts, v);
}

static unsigned com_addname(struct py_compiler* c, struct py_object* v) {
	return com_add(c->names, v);
}

static void com_addopname(
		struct py_compiler* c, py_byte_t op, struct py_node* n) {

	struct py_object* v;
	int i;
	char* name;
	if(n->type == PY_STAR) {
		name = "*";
	}
	else {
		PY_REQ(n, PY_NAME);
		name = n->str;
	}
	if((v = py_string_new(name)) == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		i = com_addname(c, v);
		py_object_decref(v);
	}
	com_addoparg(c, op, i);
}

static struct py_object* parsenumber(char* s) {
	char* end = s;
	long x;
	x = strtol(s, &end, 0);
	if(*end == '\0') {
		return py_int_new(x);
	}
	if(*end == '.' || *end == 'e' || *end == 'E') {
		return py_float_new(atof(s));
	}
	py_error_set_string(py_runtime_error, "bad number syntax");
	return NULL;
}

static struct py_object* parsestr(const char* s) {
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

static void com_list_constructor(struct py_compiler* c, struct py_node* n) {
	unsigned len;
	unsigned i;

	if(n->type != PY_GRAMMAR_TEST_LIST) PY_REQ(n, PY_GRAMMAR_EXPRESSION_LIST);

	/* PY_GRAMMAR_EXPRESSION_LIST: PY_GRAMMAR_EXPRESSION (',' PY_GRAMMAR_EXPRESSION)* [',']; likewise for PY_GRAMMAR_TEST_LIST */
	len = (n->count + 1) / 2;

	for(i = 0; i < n->count; i += 2) com_node(c, &n->children[i]);

	com_addoparg(c, PY_OP_BUILD_LIST, len);
}

static void com_atom(struct py_compiler* c, struct py_node* n) {
	struct py_node* ch;
	struct py_object* v;
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_ATOM);
	ch = &n->children[0];

	switch(ch->type) {
		case PY_LPAR: {
			if(n->children[1].type == PY_RPAR) {
				com_addoparg(c, PY_OP_BUILD_TUPLE, 0);
			}
			else com_node(c, &n->children[1]);

			break;
		}

		case PY_LSQB: {
			if(n->children[1].type == PY_RSQB) {
				com_addoparg(c, PY_OP_BUILD_LIST, 0);
			}
			else com_list_constructor(c, &n->children[1]);

			break;
		}

		case PY_LBRACE: {
			com_addoparg(c, PY_OP_BUILD_MAP, 0);

			break;
		}

		case PY_NUMBER: {
			if((v = parsenumber(ch->str)) == NULL) {
				/* TODO: Proper EH. */
				abort();
			}
			else {
				i = com_addconst(c, v);
				py_object_decref(v);
			}

			com_addoparg(c, PY_OP_LOAD_CONST, i);

			break;
		}

		case PY_STRING: {
			if((v = parsestr(ch->str)) == NULL) {
				/* TODO: Proper EH. */
				abort();
			}
			else {
				i = com_addconst(c, v);
				py_object_decref(v);
			}

			com_addoparg(c, PY_OP_LOAD_CONST, i);

			break;
		}

		case PY_NAME: {
			com_addopname(c, PY_OP_LOAD_NAME, ch);

			break;
		}

		default: {
			fprintf(stderr, "node type %d\n", ch->type);
			py_error_set_string(
					py_system_error, "com_atom: unexpected node type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void com_slice(struct py_compiler* c, struct py_node* n, py_byte_t op) {
	if(n->count == 1) com_addbyte(c, op);
	else if(n->count == 2) {
		if(n->children[0].type != PY_COLON) {
			com_node(c, &n->children[0]);
			com_addbyte(c, op + 1);
		}
		else {
			com_node(c, &n->children[1]);
			com_addbyte(c, op + 2);
		}
	}
	else {
		com_node(c, &n->children[0]);
		com_node(c, &n->children[2]);
		com_addbyte(c, op + 3);
	}
}

static void com_apply_subscript(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_SUBSCRIPT);

	if(n->count == 1 && n->children[0].type != PY_COLON) {
		/* It's a single PY_GRAMMAR_SUBSCRIPT */
		com_node(c, &n->children[0]);
		com_addbyte(c, PY_OP_BINARY_SUBSCR);
	}
	else {
		/* It's a slice: [PY_GRAMMAR_EXPRESSION] ':' [PY_GRAMMAR_EXPRESSION] */
		com_slice(c, n, PY_OP_SLICE);
	}
}

static void com_call_function(
		struct py_compiler* c, struct py_node* n /* EITHER PY_GRAMMAR_TEST_LIST OR ')' */) {

	if(n->type == PY_RPAR) com_addbyte(c, PY_OP_UNARY_CALL);
	else {
		com_node(c, n);
		com_addbyte(c, PY_OP_BINARY_CALL);
	}
}

static void com_select_member(struct py_compiler* c, struct py_node* n) {
	com_addopname(c, PY_OP_LOAD_ATTR, n);
}

static void com_apply_trailer(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_TRAILER);

	switch(n->children[0].type) {
		case PY_LPAR: {
			com_call_function(c, &n->children[1]);

			break;
		}

		case PY_DOT: {
			com_select_member(c, &n->children[1]);

			break;
		}

		case PY_LSQB: {
			com_apply_subscript(c, &n->children[1]);

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

static void com_factor(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_FACTOR);

	if(n->children[0].type == PY_MINUS) {
		com_factor(c, &n->children[1]);
		com_addbyte(c, PY_OP_UNARY_NEGATIVE);
	}
	else {
		com_atom(c, &n->children[0]);
		for(i = 1; i < n->count; i++) com_apply_trailer(c, &n->children[i]);
	}
}

static void com_term(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	unsigned op;

	PY_REQ(n, PY_GRAMMAR_TERM);

	com_factor(c, &n->children[0]);

	for(i = 2; i < n->count; i += 2) {
		com_factor(c, &n->children[i]);

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
						"com_term: PY_GRAMMAR_TERM operator not *, / or %");
				/* TODO: Proper EH. */
				abort();
			}
		}

		com_addbyte(c, op);
	}
}

static void com_expr(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	py_byte_t op;

	PY_REQ(n, PY_GRAMMAR_EXPRESSION);

	com_term(c, &n->children[0]);

	for(i = 2; i < n->count; i += 2) {
		com_term(c, &n->children[i]);

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
						py_system_error, "com_expr: PY_GRAMMAR_EXPRESSION operator not + or -");
				/* TODO: Proper EH. */
				abort();
			}
		}

		com_addbyte(c, op);
	}
}

static enum py_cmp_op cmp_type(struct py_node* n) {
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

static void com_comparison(struct py_compiler* c, struct py_node* n) {
	enum py_cmp_op op;
	unsigned anchor = 0;
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_TEST_COMPARE); /* PY_GRAMMAR_TEST_COMPARE: PY_GRAMMAR_EXPRESSION (PY_GRAMMAR_COMPARE_OP PY_GRAMMAR_EXPRESSION)* */

	com_expr(c, &n->children[0]);
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
		com_expr(c, &n->children[i]);

		if(i + 2 < n->count) {
			com_addbyte(c, PY_OP_DUP_TOP);
			com_addbyte(c, PY_OP_ROT_THREE);
		}

		op = cmp_type(&n->children[i - 1]);
		if(op == PY_CMP_BAD) {
			py_error_set_string(
					py_system_error, "com_comparison: unknown PY_GRAMMAR_TEST_COMPARE op");
			/* TODO: Proper EH. */
			abort();
		}

		com_addoparg(c, PY_OP_COMPARE_OP, op);
		if(i + 2 < n->count) {
			com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
			com_addbyte(c, PY_OP_POP_TOP);
		}
	}

	if(anchor) {
		unsigned anchor2 = 0;

		com_addfwref(c, PY_OP_JUMP_FORWARD, &anchor2);
		com_backpatch(c, anchor);
		com_addbyte(c, PY_OP_ROT_TWO);
		com_addbyte(c, PY_OP_POP_TOP);
		com_backpatch(c, anchor2);
	}
}

static void com_not_test(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_TEST_NOT); /* 'not' PY_GRAMMAR_TEST_NOT | PY_GRAMMAR_TEST_COMPARE */

	if(n->count == 1) com_comparison(c, &n->children[0]);
	else {
		com_not_test(c, &n->children[1]);
		com_addbyte(c, PY_OP_UNARY_NOT);
	}
}

static void com_and_test(struct py_compiler* c, struct py_node* n) {
	unsigned i = 0;
	unsigned anchor = 0;

	PY_REQ(n, PY_GRAMMAR_TEST_AND); /* PY_GRAMMAR_TEST_NOT ('and' PY_GRAMMAR_TEST_NOT)* */

	for(;;) {
		com_not_test(c, &n->children[i]);

		if((i += 2) >= n->count) break;

		com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
		com_addbyte(c, PY_OP_POP_TOP);
	}

	if(anchor) com_backpatch(c, anchor);
}

static void com_test(struct py_compiler* c, struct py_node* n) {
	unsigned i = 0;
	unsigned anchor = 0;

	PY_REQ(n, PY_GRAMMAR_TEST); /* PY_GRAMMAR_TEST_AND ('and' PY_GRAMMAR_TEST_AND)* */

	for(;;) {
		com_and_test(c, &n->children[i]);

		if((i += 2) >= n->count) break;

		com_addfwref(c, PY_OP_JUMP_IF_TRUE, &anchor);
		com_addbyte(c, PY_OP_POP_TOP);
	}

	if(anchor) com_backpatch(c, anchor);
}

static void com_list(struct py_compiler* c, struct py_node* n) {
	/* PY_GRAMMAR_EXPRESSION_LIST: PY_GRAMMAR_EXPRESSION (',' PY_GRAMMAR_EXPRESSION)* [',']; likewise for PY_GRAMMAR_TEST_LIST */
	if(n->count == 1) com_node(c, &n->children[0]);
	else {
		unsigned i;
		unsigned len = (n->count + 1) / 2;

		for(i = 0; i < n->count; i += 2) com_node(c, &n->children[i]);

		com_addoparg(c, PY_OP_BUILD_TUPLE, len);
	}
}


/* Begin of assignment compilation */

static void com_assign_trailer(
		struct py_compiler* c, struct py_node* n, int assigning) {

	PY_REQ(n, PY_GRAMMAR_TRAILER);

	switch(n->children[0].type) {
		case PY_LPAR: { /* '(' [PY_GRAMMAR_EXPRESSION_LIST] ')' */
			py_error_set_string(
					py_type_error, "can't assign to function call");
			/* TODO: Proper EH. */
			abort();
		}

		case PY_DOT: { /* '.' PY_NAME */
			com_addopname(
					c, assigning ? PY_OP_STORE_ATTR : PY_OP_DELETE_ATTR,
					&n->children[1]);

			break;
		}

		case PY_LSQB: { /* '[' PY_GRAMMAR_SUBSCRIPT ']' */
			n = &n->children[1];

			PY_REQ(n, PY_GRAMMAR_SUBSCRIPT); /* PY_GRAMMAR_SUBSCRIPT: PY_GRAMMAR_EXPRESSION | [PY_GRAMMAR_EXPRESSION] ':' [PY_GRAMMAR_EXPRESSION] */

			com_node(c, &n->children[0]);
			com_addbyte(
					c, assigning ? PY_OP_STORE_SUBSCR : PY_OP_DELETE_SUBSCR);

			break;
		}

		default: {
			py_error_set_string(py_type_error, "unknown PY_GRAMMAR_TRAILER type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void com_assign(
		struct py_compiler* c, struct py_node* n, int assigning);

static void com_assign_tuple(
		struct py_compiler* c, struct py_node* n, int assigning) {

	unsigned i;

	if(n->type != PY_GRAMMAR_TEST_LIST) PY_REQ(n, PY_GRAMMAR_EXPRESSION_LIST);

	if(assigning) com_addoparg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);

	for(i = 0; i < n->count; i += 2) com_assign(c, &n->children[i], assigning);
}

static void com_assign_list(
		struct py_compiler* c, struct py_node* n, int assigning) {

	unsigned i;
	if(assigning) com_addoparg(c, PY_OP_UNPACK_LIST, (n->count + 1) / 2);

	for(i = 0; i < n->count; i += 2) com_assign(c, &n->children[i], assigning);
}

static void com_assign_name(
		struct py_compiler* c, struct py_node* n, int assigning) {

	PY_REQ(n, PY_NAME);
	com_addopname(c, assigning ? PY_OP_STORE_NAME : PY_OP_DELETE_NAME, n);
}

static void com_assign(
		struct py_compiler* c, struct py_node* n, int assigning) {

	/* Loop to avoid trivial recursion */
	for(;;) {
		switch(n->type) {
			case PY_GRAMMAR_EXPRESSION_LIST: {
				PY_FALLTHROUGH;
				/* FALLTHRU */
			}
			case PY_GRAMMAR_TEST_LIST: {
				if(n->count > 1) {
					com_assign_tuple(c, n, assigning);
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

					com_node(c, &n->children[0]);
					for(i = 1; i + 1 < n->count; i++) {
						com_apply_trailer(c, &n->children[i]);
					} /* NB i is still alive */

					com_assign_trailer(
							c, &n->children[i], assigning);

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

						com_assign_list(c, n, assigning);

						return;
					}

					case PY_NAME: {
						com_assign_name(c, &n->children[0], assigning);

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
				py_error_set_string(py_system_error, "com_assign: bad node");
				/* TODO: Proper EH. */
				abort();
			}
		}
	}
}

static void com_expr_stmt(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_EXPRESSION_STATEMENT); /* PY_GRAMMAR_EXPRESSION_LIST ('=' PY_GRAMMAR_EXPRESSION_LIST)* PY_NEWLINE */

	com_node(c, &n->children[n->count - 2]);
	if(n->count == 2) com_addbyte(c, PY_OP_PRINT_EXPR);
	else {
		unsigned i;
		for(i = 0; i < n->count - 3; i += 2) {
			if(i + 2 < n->count - 3) com_addbyte(c, PY_OP_DUP_TOP);
			com_assign(c, &n->children[i], 1/*assign*/);
		}
	}
}

static void com_return_stmt(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_RETURN_STATEMENT); /* 'return' [PY_GRAMMAR_TEST_LIST] PY_NEWLINE */

	if(!c->in_function) {
		py_error_set_string(py_type_error, "'return' outside function");
		/* TODO: Proper EH. */
		abort();
	}

	if(n->count == 2) com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	else com_node(c, &n->children[1]);

	com_addbyte(c, PY_OP_RETURN_VALUE);
}

static void com_raise_stmt(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_RAISE_STATEMENT); /* 'raise' PY_GRAMMAR_EXPRESSION [',' PY_GRAMMAR_EXPRESSION] PY_NEWLINE */

	com_node(c, &n->children[1]);

	if(n->count > 3) com_node(c, &n->children[3]);
	else com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));

	com_addbyte(c, PY_OP_RAISE_EXCEPTION);
}

static void com_import_stmt(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_IMPORT_STATEMENT);

	/*
	 * 'import' PY_NAME (',' PY_NAME)* PY_NEWLINE |
	 * 'from' PY_NAME 'import' ('*' | PY_NAME (',' PY_NAME)*) PY_NEWLINE
	 */
	if(n->children[0].str[0] == 'f') {
		/* 'from' PY_NAME 'import' ... */
		PY_REQ(&n->children[1], PY_NAME);

		com_addopname(c, PY_OP_IMPORT_NAME, &n->children[1]);

		for(i = 3; i < n->count; i += 2) {
			com_addopname(c, PY_OP_IMPORT_FROM, &n->children[i]);
		}

		com_addbyte(c, PY_OP_POP_TOP);
	}
	else {
		/* 'import' ... */
		for(i = 1; i < n->count; i += 2) {
			com_addopname(c, PY_OP_IMPORT_NAME, &n->children[i]);
			com_addopname(c, PY_OP_STORE_NAME, &n->children[i]);
		}
	}
}

static void com_if_stmt(struct py_compiler* c, struct py_node* n) {
	unsigned i;
	unsigned anchor = 0;

	PY_REQ(n, PY_GRAMMAR_IF_STATEMENT);
	/*'if' PY_GRAMMAR_TEST ':' PY_GRAMMAR_SUITE ('elif' PY_GRAMMAR_TEST ':' PY_GRAMMAR_SUITE)* ['else' ':' PY_GRAMMAR_SUITE] */

	for(i = 0; i + 3 < n->count; i += 4) {
		unsigned a = 0;
		struct py_node* ch = &n->children[i + 1];

		if(i > 0) com_addoparg(c, PY_OP_SET_LINENO, ch->lineno);

		com_node(c, &n->children[i + 1]);
		com_addfwref(c, PY_OP_JUMP_IF_FALSE, &a);
		com_addbyte(c, PY_OP_POP_TOP);

		com_node(c, &n->children[i + 3]);
		com_addfwref(c, PY_OP_JUMP_FORWARD, &anchor);
		com_backpatch(c, a);
		com_addbyte(c, PY_OP_POP_TOP);
	}

	if(i + 2 < n->count) com_node(c, &n->children[i + 2]);

	com_backpatch(c, anchor);
}

static void com_while_stmt(struct py_compiler* c, struct py_node* n) {
	unsigned break_anchor = 0;
	unsigned anchor = 0;
	unsigned begin;

	PY_REQ(n, PY_GRAMMAR_WHILE_STATEMENT); /* 'while' PY_GRAMMAR_TEST ':' PY_GRAMMAR_SUITE ['else' ':' PY_GRAMMAR_SUITE] */

	com_addfwref(c, PY_OP_SETUP_LOOP, &break_anchor);

	begin = c->offset;

	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
	com_node(c, &n->children[1]);
	com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
	com_addbyte(c, PY_OP_POP_TOP);

	c->nesting++;
	com_node(c, &n->children[3]);
	c->nesting--;

	com_addoparg(c, PY_OP_JUMP_ABSOLUTE, begin);
	com_backpatch(c, anchor);
	com_addbyte(c, PY_OP_POP_TOP);
	com_addbyte(c, PY_OP_POP_BLOCK);

	if(n->count > 4) com_node(c, &n->children[6]);

	com_backpatch(c, break_anchor);
}

static void com_for_stmt(struct py_compiler* c, struct py_node* n) {
	struct py_object* v;
	unsigned break_anchor = 0;
	unsigned anchor = 0;
	unsigned begin;

	PY_REQ(n, PY_GRAMMAR_FOR_STATEMENT);

	/* 'for' PY_GRAMMAR_EXPRESSION_LIST 'in' PY_GRAMMAR_EXPRESSION_LIST ':' PY_GRAMMAR_SUITE ['else' ':' PY_GRAMMAR_SUITE] */
	com_addfwref(c, PY_OP_SETUP_LOOP, &break_anchor);
	com_node(c, &n->children[3]);

	v = py_int_new(0);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}

	com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, v));
	py_object_decref(v);

	begin = c->offset;

	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
	com_addfwref(c, PY_OP_FOR_LOOP, &anchor);
	com_assign(c, &n->children[1], 1/*assigning*/);

	c->nesting++;
	com_node(c, &n->children[5]);
	c->nesting--;

	com_addoparg(c, PY_OP_JUMP_ABSOLUTE, begin);
	com_backpatch(c, anchor);
	com_addbyte(c, PY_OP_POP_BLOCK);

	if(n->count > 8) com_node(c, &n->children[8]);

	com_backpatch(c, break_anchor);
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

static void com_try_stmt(struct py_compiler* c, struct py_node* n) {
	unsigned finally_anchor = 0;
	unsigned except_anchor = 0;

	PY_REQ(n, PY_GRAMMAR_TRY_STATEMENT);
	/* 'try' ':' PY_GRAMMAR_SUITE (PY_GRAMMAR_EXCEPT_CLAUSE ':' PY_GRAMMAR_SUITE)* ['finally' ':' PY_GRAMMAR_SUITE] */

	if(n->count > 3 && n->children[n->count - 3].type != PY_GRAMMAR_EXCEPT_CLAUSE) {
		/* Have a 'finally' clause */
		com_addfwref(c, PY_OP_SETUP_FINALLY, &finally_anchor);
	}

	if(n->count > 3 && n->children[3].type == PY_GRAMMAR_EXCEPT_CLAUSE) {
		/* Have an 'except' clause */
		com_addfwref(c, PY_OP_SETUP_EXCEPT, &except_anchor);
	}

	com_node(c, &n->children[2]);
	if(except_anchor) {
		unsigned end_anchor = 0;
		unsigned i;
		struct py_node* ch;

		com_addbyte(c, PY_OP_POP_BLOCK);
		com_addfwref(c, PY_OP_JUMP_FORWARD, &end_anchor);
		com_backpatch(c, except_anchor);

		for(i = 3;
				i < n->count && (ch = &n->children[i])->type == PY_GRAMMAR_EXCEPT_CLAUSE;
				i += 3) {

			/* PY_GRAMMAR_EXCEPT_CLAUSE: 'except' [PY_GRAMMAR_EXPRESSION [',' PY_GRAMMAR_EXPRESSION]] */
			if(except_anchor == 0) {
				py_error_set_string(
						py_type_error, "default 'except:' must be last");
				/* TODO: Proper EH. */
				abort();
			}

			except_anchor = 0;
			com_addoparg(c, PY_OP_SET_LINENO, ch->lineno);

			if(ch->count > 1) {
				com_addbyte(c, PY_OP_DUP_TOP);
				com_node(c, &ch->children[1]);
				com_addoparg(c, PY_OP_COMPARE_OP, PY_CMP_EXC_MATCH);
				com_addfwref(c, PY_OP_JUMP_IF_FALSE, &except_anchor);
				com_addbyte(c, PY_OP_POP_TOP);
			}

			com_addbyte(c, PY_OP_POP_TOP);

			if(ch->count > 3) com_assign(c, &ch->children[3], 1);
			else com_addbyte(c, PY_OP_POP_TOP);

			com_addbyte(c, PY_OP_POP_TOP);
			com_node(c, &n->children[i + 2]);
			com_addfwref(c, PY_OP_JUMP_FORWARD, &end_anchor);

			if(except_anchor) {
				com_backpatch(c, except_anchor);
				com_addbyte(c, PY_OP_POP_TOP);
			}
		}

		com_addbyte(c, PY_OP_END_FINALLY);
		com_backpatch(c, end_anchor);
	}

	if(finally_anchor) {
		struct py_node* ch;

		com_addbyte(c, PY_OP_POP_BLOCK);
		com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
		com_backpatch(c, finally_anchor);

		ch = &n->children[n->count - 1];

		com_addoparg(c, PY_OP_SET_LINENO, ch->lineno);
		com_node(c, ch);
		com_addbyte(c, PY_OP_END_FINALLY);
	}
}

static void com_suite(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_SUITE);

	/* PY_GRAMMAR_SIMPLE_STATEMENT | PY_NEWLINE PY_INDENT PY_NEWLINE* (PY_GRAMMAR_STATEMENT PY_NEWLINE*)+ PY_DEDENT */
	if(n->count == 1) com_node(c, &n->children[0]);
	else {
		unsigned i;
		for(i = 0; i < n->count; i++) {
			struct py_node* ch = &n->children[i];

			if(ch->type == PY_GRAMMAR_STATEMENT) com_node(c, ch);
		}
	}
}

static void com_funcdef(struct py_compiler* c, struct py_node* n) {
	struct py_object* v;

	PY_REQ(n, PY_GRAMMAR_FUNCTION_DEFINITION);
	/* PY_GRAMMAR_FUNCTION_DEFINITION: 'def' PY_NAME parameters ':' PY_GRAMMAR_SUITE */

	v = (struct py_object*) py_compile(n, c->filename);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, v));
		com_addbyte(c, PY_OP_BUILD_FUNCTION);
		com_addopname(c, PY_OP_STORE_NAME, &n->children[1]);
		py_object_decref(v);
	}
}

static void com_bases(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_BASE_LIST);
	/*
	 * PY_GRAMMAR_BASE_LIST: PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS (',' PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS)*
	 * PY_GRAMMAR_ARGUMENTS: '(' [PY_GRAMMAR_TEST_LIST] ')'
	 */
	for(i = 0; i < n->count; i += 3) com_node(c, &n->children[i]);

	com_addoparg(c, PY_OP_BUILD_TUPLE, (n->count + 1) / 3);
}

static void com_classdef(struct py_compiler* c, struct py_node* n) {
	struct py_object* v;

	PY_REQ(n, PY_GRAMMAR_CLASS_DEFINITION);
	/*
	 * PY_GRAMMAR_CLASS_DEFINITION: 'class' PY_NAME parameters ['=' PY_GRAMMAR_BASE_LIST] ':' PY_GRAMMAR_SUITE
	 * PY_GRAMMAR_BASE_LIST: PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS (',' PY_GRAMMAR_ATOM PY_GRAMMAR_ARGUMENTS)*
	 * PY_GRAMMAR_ARGUMENTS: '(' [PY_GRAMMAR_TEST_LIST] ')'
     */

	if(n->count == 7) com_bases(c, &n->children[4]);
	else com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));

	v = (struct py_object*) py_compile(n, c->filename);
	if(v == NULL) {
		/* TODO: Proper EH. */
		abort();
	}
	else {
		int i = com_addconst(c, v);
		com_addoparg(c, PY_OP_LOAD_CONST, i);
		com_addbyte(c, PY_OP_BUILD_FUNCTION);
		com_addbyte(c, PY_OP_UNARY_CALL);
		com_addbyte(c, PY_OP_BUILD_CLASS);
		com_addopname(c, PY_OP_STORE_NAME, &n->children[1]);
		py_object_decref(v);
	}
}

static void com_node(struct py_compiler* c, struct py_node* n) {
	switch(n->type) {
		/* Definition nodes */

		case PY_GRAMMAR_FUNCTION_DEFINITION: {
			com_funcdef(c, n);

			break;
		}

		case PY_GRAMMAR_CLASS_DEFINITION: {
			com_classdef(c, n);

			break;
		}

		/* Trivial parse tree nodes */

		case PY_GRAMMAR_STATEMENT: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_GRAMMAR_FLOW_STATEMENT: {
			com_node(c, &n->children[0]);
			break;
		}

		case PY_GRAMMAR_SIMPLE_STATEMENT: {
			PY_FALLTHROUGH;
			/* FALLTHRU */
		}
		case PY_GRAMMAR_COMPOUND_STATEMENT: {
			com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
			com_node(c, &n->children[0]);

			break;
		}

		/* Statement nodes */

		case PY_GRAMMAR_EXPRESSION_STATEMENT: {
			com_expr_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_BREAK_STATEMENT: {
			if(c->nesting == 0) {
				py_error_set_string(py_type_error, "'break' outside loop");
				/* TODO: Proper EH. */
				abort();
			}

			com_addbyte(c, PY_OP_BREAK_LOOP);

			break;
		}

		case PY_GRAMMAR_RETURN_STATEMENT: {
			com_return_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_RAISE_STATEMENT: {
			com_raise_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_IMPORT_STATEMENT: {
			com_import_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_IF_STATEMENT: {
			com_if_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_WHILE_STATEMENT: {
			com_while_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_FOR_STATEMENT: {
			com_for_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_TRY_STATEMENT: {
			com_try_stmt(c, n);

			break;
		}

		case PY_GRAMMAR_SUITE: {
			com_suite(c, n);

			break;
		}

		/* Expression nodes */

		case PY_GRAMMAR_TEST_LIST: {
			com_list(c, n);

			break;
		}

		case PY_GRAMMAR_TEST: {
			com_test(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_AND: {
			com_and_test(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_NOT: {
			com_not_test(c, n);

			break;
		}

		case PY_GRAMMAR_TEST_COMPARE: {
			com_comparison(c, n);

			break;
		}

		case PY_GRAMMAR_EXPRESSION_LIST: {
			com_list(c, n);

			break;
		}

		case PY_GRAMMAR_EXPRESSION: {
			com_expr(c, n);

			break;
		}

		case PY_GRAMMAR_TERM: {
			com_term(c, n);

			break;
		}

		case PY_GRAMMAR_FACTOR: {
			com_factor(c, n);

			break;
		}

		case PY_GRAMMAR_ATOM: {
			com_atom(c, n);

			break;
		}

		default: {
			fprintf(stderr, "node type %d\n", n->type);
			py_error_set_string(
					py_system_error, "com_node: unexpected node type");
			/* TODO: Proper EH. */
			abort();
		}
	}
}

static void com_fplist(struct py_compiler*, struct py_node*);

static void com_fpdef(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_PARAMETER_DEFINITION); /* PY_GRAMMAR_PARAMETER_DEFINITION: PY_NAME | '(' PY_GRAMMAR_PARAMETER_LIST ')' */

	if(n->children[0].type == PY_LPAR) com_fplist(c, &n->children[1]);
	else com_addopname(c, PY_OP_STORE_NAME, &n->children[0]);
}

static void com_fplist(struct py_compiler* c, struct py_node* n) {
	PY_REQ(n, PY_GRAMMAR_PARAMETER_LIST); /* PY_GRAMMAR_PARAMETER_LIST: PY_GRAMMAR_PARAMETER_DEFINITION (',' PY_GRAMMAR_PARAMETER_DEFINITION)* */

	if(n->count == 1) com_fpdef(c, &n->children[0]);
	else {
		unsigned i;

		com_addoparg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);
		for(i = 0; i < n->count; i += 2) com_fpdef(c, &n->children[i]);
	}
}

static void com_file_input(struct py_compiler* c, struct py_node* n) {
	unsigned i;

	PY_REQ(n, PY_GRAMMAR_FILE_INPUT); /* (PY_NEWLINE | PY_GRAMMAR_STATEMENT)* PY_ENDMARKER */

	for(i = 0; i < n->count; i++) {
		struct py_node* ch = &n->children[i];

		if(ch->type != PY_ENDMARKER && ch->type != PY_NEWLINE) com_node(c, ch);
	}
}

/* Top-level py_compile-node interface */

static void compile_funcdef(struct py_compiler* c, struct py_node* n) {
	struct py_node* ch;

	/* PY_GRAMMAR_FUNCTION_DEFINITION: 'def' PY_NAME parameters ':' PY_GRAMMAR_SUITE */
	PY_REQ(n, PY_GRAMMAR_FUNCTION_DEFINITION);

	ch = &n->children[2]; /* parameters: '(' [PY_GRAMMAR_PARAMETER_LIST] ')' */
	ch = &ch->children[1]; /* ')' | PY_GRAMMAR_PARAMETER_LIST */

	if(ch->type == PY_RPAR) com_addbyte(c, PY_OP_REFUSE_ARGS);
	else {
		com_addbyte(c, PY_OP_REQUIRE_ARGS);
		com_fplist(c, ch);
	}

	c->in_function = 1;
	com_node(c, &n->children[4]);
	c->in_function = 0;

	com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	com_addbyte(c, PY_OP_RETURN_VALUE);
}

static void compile_node(struct py_compiler* c, struct py_node* n) {
	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);

	switch(n->type) {
		/* A whole file. */
		case PY_GRAMMAR_FILE_INPUT: {
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_file_input(c, n);
			com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
			com_addbyte(c, PY_OP_RETURN_VALUE);

			break;
		}

		/* A function definition */
		case PY_GRAMMAR_FUNCTION_DEFINITION: {
			compile_funcdef(c, n);

			break;
		}

		case PY_GRAMMAR_CLASS_DEFINITION: { /* A class definition */
			/* 'class' PY_NAME parameters ['=' PY_GRAMMAR_BASE_LIST] ':' PY_GRAMMAR_SUITE */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_node(c, &n->children[n->count - 1]);
			com_addbyte(c, PY_OP_LOAD_LOCALS);
			com_addbyte(c, PY_OP_RETURN_VALUE);

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

	if(!com_init(&sc, filename)) return NULL;

	compile_node(&sc, n);
	com_done(&sc);
	co = py_code_new(sc.code, sc.consts, sc.names, filename);

	com_free(&sc);
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
		{ 1, 0, &py_type_type }, "code", sizeof(struct py_code),
		code_dealloc, /* dealloc */
		0, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, /* sequencemethods */
};
