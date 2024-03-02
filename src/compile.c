/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Compile an expression node to intermediate code */

/* XXX TO DO:
   XXX Compute maximum needed stack sizes while compiling
   XXX Generate simple jump for break/return outside 'try...finally'
   XXX Include function name in code (and module names?)
*/

#include <python/std.h>
#include <python/node.h>
#include <python/token.h>
#include <python/graminit.h>
#include <python/opcode.h>
#include <python/structmember.h>
#include <python/compile.h>
#include <python/errors.h>

#include <python/listobject.h>
#include <python/intobject.h>
#include <python/floatobject.h>

#define OFF(x) offsetof(struct py_code, x)

static struct py_memberlist code_memberlist[] = {
		{ "code",     PY_TYPE_OBJECT, OFF(code),     PY_READWRITE },
		{ "consts",   PY_TYPE_OBJECT, OFF(consts),   PY_READWRITE },
		{ "names",    PY_TYPE_OBJECT, OFF(names),    PY_READWRITE },
		{ "filename", PY_TYPE_OBJECT, OFF(filename), PY_READWRITE },
		{ NULL,       0, 0,                          0 }  /* Sentinel */
};

static struct py_object* code_getattr(co, name)struct py_code* co;
											   char* name;
{
	return py_memberlist_get((char*) co, code_memberlist, name);
}

static void code_dealloc(co)struct py_code* co;
{
	py_object_decref(co->code);
	py_object_decref(co->consts);
	py_object_decref(co->names);
	py_object_decref(co->filename);
	free(co);
}

struct py_type py_code_type = {
		{ 1, &py_type_type, 0 }, "code", sizeof(struct py_code), 0,
		code_dealloc, /* dealloc */
		0, /* print */
		code_getattr, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, /* repr */
		0, /* numbermethods */
		0, /* sequencemethods */
		0, /* mappingmethods */
};

static struct py_code*
newcodeobject(struct py_object*, struct py_object*, struct py_object*, char*);

static struct py_code* newcodeobject(code, consts, names, filename)
		struct py_object* code;
		struct py_object* consts;
		struct py_object* names;
		char* filename;
{
	struct py_code* co;
	int i;
	/* Check argument types */
	if(code == NULL || !py_is_string(code) || consts == NULL ||
	   !py_is_list(consts) || names == NULL || !py_is_list(names)) {
		py_error_set_badcall();
		return NULL;
	}
	/* Make sure the list of names contains only strings */
	for(i = py_varobject_size(names); --i >= 0;) {
		struct py_object* v = py_list_get(names, i);
		if(v == NULL || !py_is_string(v)) {
			py_error_set_badcall();
			return NULL;
		}
	}
	co = py_object_new(&py_code_type);
	if(co != NULL) {
		py_object_incref(code);
		co->code = (struct py_string*) code;
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


/* Data structure used internally */
struct compiling {
	struct py_object* c_code;         /* string */
	struct py_object* c_consts;       /* list of objects */
	struct py_object* c_names;        /* list of strings (names) */
	int c_nexti;            /* index into c_code */
	int c_errors;           /* counts errors occurred */
	int c_infunction;       /* set when compiling a function */
	int c_loops;            /* counts nested loops */
	char* c_filename;       /* filename of current node */
};

/* Prototypes */
static int com_init(struct compiling*, char*);

static void com_free(struct compiling*);

static void com_done(struct compiling*);

static void com_node(struct compiling*, struct py_node*);

static void com_addbyte(struct compiling*, int);

static void com_addint(struct compiling*, int);

static void com_addoparg(struct compiling*, int, int);

static void com_addfwref(struct compiling*, int, int*);

static void com_backpatch(struct compiling*, int);

static int com_add(struct compiling*, struct py_object*, struct py_object*);

static int com_addconst(struct compiling*, struct py_object*);

static int com_addname(struct compiling*, struct py_object*);

static void com_addopname(struct compiling*, int, struct py_node*);

static int com_init(c, filename)struct compiling* c;
								char* filename;
{
	if((c->c_code = py_string_new_size((char*) NULL, 0)) == NULL) {
		goto fail_3;
	}
	if((c->c_consts = py_list_new(0)) == NULL) {
		goto fail_2;
	}
	if((c->c_names = py_list_new(0)) == NULL) {
		goto fail_1;
	}
	c->c_nexti = 0;
	c->c_errors = 0;
	c->c_infunction = 0;
	c->c_loops = 0;
	c->c_filename = filename;
	return 1;

	fail_1:
	py_object_decref(c->c_consts);
	fail_2:
	py_object_decref(c->c_code);
	fail_3:
	return 0;
}

static void com_free(c)struct compiling* c;
{
	py_object_decref(c->c_code);
	py_object_decref(c->c_consts);
	py_object_decref(c->c_names);
}

static void com_done(c)struct compiling* c;
{
	if(c->c_code != NULL) {
		py_string_resize(&c->c_code, c->c_nexti);
	}
}

static void com_addbyte(c, byte)struct compiling* c;
								int byte;
{
	int len;
	if(byte < 0 || byte > 255) {
		fprintf(stderr, "XXX compiling bad byte: %d\n", byte);
		abort();
		py_error_set_string(py_system_error, "com_addbyte: byte out of range");
		c->c_errors++;
	}
	if(c->c_code == NULL) {
		return;
	}
	len = py_varobject_size(c->c_code);
	if(c->c_nexti >= len) {
		if(py_string_resize(&c->c_code, len + 1000) != 0) {
			c->c_errors++;
			return;
		}
	}
	py_string_get_value(c->c_code)[c->c_nexti++] = byte;
}

static void com_addint(c, x)struct compiling* c;
							int x;
{
	com_addbyte(c, x & 0xff);
	com_addbyte(c, x >> 8); /* XXX x should be positive */
}

static void com_addoparg(c, op, arg)struct compiling* c;
									int op;
									int arg;
{
	com_addbyte(c, op);
	com_addint(c, arg);
}

static void com_addfwref(c, op, p_anchor)struct compiling* c;
										 int op;
										 int* p_anchor;
{
	/* Compile a forward reference for backpatching */
	int here;
	int anchor;
	com_addbyte(c, op);
	here = c->c_nexti;
	anchor = *p_anchor;
	*p_anchor = here;
	com_addint(c, anchor == 0 ? 0 : here - anchor);
}

static void com_backpatch(c, anchor)struct compiling* c;
									int anchor; /* Must be nonzero */
{
	unsigned char* code = (unsigned char*) py_string_get_value(c->c_code);
	int target = c->c_nexti;
	int dist;
	int prev;
	for(;;) {
		/* Make the JUMP instruction at anchor point to target */
		prev = code[anchor] + (code[anchor + 1] << 8);
		dist = target - (anchor + 2);
		code[anchor] = dist & 0xff;
		code[anchor + 1] = dist >> 8;
		if(!prev) {
			break;
		}
		anchor -= prev;
	}
}

/* Handle constants and names uniformly */

static int com_add(c, list, v)struct compiling* c;
							  struct py_object* list;
							  struct py_object* v;
{
	int n = py_varobject_size(list);
	int i;
	for(i = n; --i >= 0;) {
		struct py_object* w = py_list_get(list, i);
		if(py_object_cmp(v, w) == 0) {
			return i;
		}
	}
	if(py_list_add(list, v) != 0) {
		c->c_errors++;
	}
	return n;
}

static int com_addconst(c, v)struct compiling* c;
							 struct py_object* v;
{
	return com_add(c, c->c_consts, v);
}

static int com_addname(c, v)struct compiling* c;
							struct py_object* v;
{
	return com_add(c, c->c_names, v);
}

static void com_addopname(c, op, n)struct compiling* c;
								   int op;
								   struct py_node* n;
{
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
		c->c_errors++;
		i = 255;
	}
	else {
		i = com_addname(c, v);
		py_object_decref(v);
	}
	com_addoparg(c, op, i);
}

static struct py_object* parsenumber(s)char* s;
{
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

static struct py_object* parsestr(s)char* s;
{
	struct py_object* v;
	int len;
	char* buf;
	char* p;
	int c;
	if(*s != '\'') {
		py_error_set_badcall();
		return NULL;
	}
	s++;
	len = strlen(s);
	if(s[--len] != '\'') {
		py_error_set_badcall();
		return NULL;
	}
	if(strchr(s, '\\') == NULL) {
		return py_string_new_size(s, len);
	}
	v = py_string_new_size((char*) NULL, len);
	p = buf = py_string_get_value(v);
	while(*s != '\0' && *s != '\'') {
		if(*s != '\\') {
			*p++ = *s++;
			continue;
		}
		s++;
		switch(*s++) {
			/* XXX This assumes ASCII! */
			case '\\': *p++ = '\\';
				break;
			case '\'': *p++ = '\'';
				break;
			case 'b': *p++ = '\b';
				break;
			case 'f': *p++ = '\014';
				break; /* FF */
			case 't': *p++ = '\t';
				break;
			case 'n': *p++ = '\n';
				break;
			case 'r': *p++ = '\r';
				break;
			case 'v': *p++ = '\013';
				break; /* VT */
			case 'E': *p++ = '\033';
				break; /* ESC, not C */
			case 'a': *p++ = '\007';
				break; /* BEL, not classic C */
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':c = s[-1] - '0';
				if('0' <= *s && *s <= '7') {
					c = (c << 3) + *s++ - '0';
					if('0' <= *s && *s <= '7') {
						c = (c << 3) + *s++ - '0';
					}
				}
				*p++ = c;
				break;
			case 'x':
				if(isxdigit(*s)) {
					sscanf(s, "%x", (unsigned*) &c);
					*p++ = c;
					do {
						s++;
					} while(isxdigit(*s));
					break;
				}
				/* FALLTHROUGH */
			default: *p++ = '\\';
				*p++ = s[-1];
				break;
		}
	}
	py_string_resize(&v, (int) (p - buf));
	return v;
}

static void com_list_constructor(c, n)struct compiling* c;
									  struct py_node* n;
{
	int len;
	int i;
	if(n->type != testlist) PY_REQ(n, exprlist);
	/* exprlist: expr (',' expr)* [',']; likewise for testlist */
	len = (n->count + 1) / 2;
	for(i = 0; i < n->count; i += 2) {
		com_node(c, &n->children[i]);
	}
	com_addoparg(c, PY_OP_BUILD_LIST, len);
}

static void com_atom(c, n)struct compiling* c;
						  struct py_node* n;
{
	struct py_node* ch;
	struct py_object* v;
	int i;
	PY_REQ(n, atom);
	ch = &n->children[0];
	switch(ch->type) {
		case PY_LPAR:
			if(n->children[1].type == PY_RPAR) {
				com_addoparg(c, PY_OP_BUILD_TUPLE, 0);
			}
			else {
				com_node(c, &n->children[1]);
			}
			break;
		case PY_LSQB:
			if(n->children[1].type == PY_RSQB) {
				com_addoparg(c, PY_OP_BUILD_LIST, 0);
			}
			else {
				com_list_constructor(c, &n->children[1]);
			}
			break;
		case PY_LBRACE:com_addoparg(c, PY_OP_BUILD_MAP, 0);
			break;
		case PY_BACKQUOTE:com_node(c, &n->children[1]);
			com_addbyte(c, PY_OP_UNARY_CONVERT);
			break;
		case PY_NUMBER:
			if((v = parsenumber(ch->str)) == NULL) {
				c->c_errors++;
				i = 255;
			}
			else {
				i = com_addconst(c, v);
				py_object_decref(v);
			}
			com_addoparg(c, PY_OP_LOAD_CONST, i);
			break;
		case PY_STRING:
			if((v = parsestr(ch->str)) == NULL) {
				c->c_errors++;
				i = 255;
			}
			else {
				i = com_addconst(c, v);
				py_object_decref(v);
			}
			com_addoparg(c, PY_OP_LOAD_CONST, i);
			break;
		case PY_NAME:com_addopname(c, PY_OP_LOAD_NAME, ch);
			break;
		default:fprintf(stderr, "node type %d\n", ch->type);
			py_error_set_string(
					py_system_error, "com_atom: unexpected node type");
			c->c_errors++;
	}
}

static void com_slice(c, n, op)struct compiling* c;
							   struct py_node* n;
							   int op;
{
	if(n->count == 1) {
		com_addbyte(c, op);
	}
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

static void com_apply_subscript(c, n)struct compiling* c;
									 struct py_node* n;
{
	PY_REQ(n, subscript);
	if(n->count == 1 && n->children[0].type != PY_COLON) {
		/* It's a single subscript */
		com_node(c, &n->children[0]);
		com_addbyte(c, PY_OP_BINARY_SUBSCR);
	}
	else {
		/* It's a slice: [expr] ':' [expr] */
		com_slice(c, n, PY_OP_SLICE);
	}
}

static void com_call_function(c, n)struct compiling* c;
								   struct py_node* n; /* EITHER testlist OR ')' */
{
	if(n->type == PY_RPAR) {
		com_addbyte(c, PY_OP_UNARY_CALL);
	}
	else {
		com_node(c, n);
		com_addbyte(c, PY_OP_BINARY_CALL);
	}
}

static void com_select_member(c, n)struct compiling* c;
								   struct py_node* n;
{
	com_addopname(c, PY_OP_LOAD_ATTR, n);
}

static void com_apply_trailer(c, n)struct compiling* c;
								   struct py_node* n;
{
	PY_REQ(n, trailer);
	switch(n->children[0].type) {
		case PY_LPAR:com_call_function(c, &n->children[1]);
			break;
		case PY_DOT:com_select_member(c, &n->children[1]);
			break;
		case PY_LSQB:com_apply_subscript(c, &n->children[1]);
			break;
		default:
			py_error_set_string(
					py_system_error, "com_apply_trailer: unknown trailer type");
			c->c_errors++;
	}
}

static void com_factor(c, n)struct compiling* c;
							struct py_node* n;
{
	int i;
	PY_REQ(n, factor);
	if(n->children[0].type == PY_PLUS) {
		com_factor(c, &n->children[1]);
		com_addbyte(c, PY_OP_UNARY_POSITIVE);
	}
	else if(n->children[0].type == PY_MINUS) {
		com_factor(c, &n->children[1]);
		com_addbyte(c, PY_OP_UNARY_NEGATIVE);
	}
	else {
		com_atom(c, &n->children[0]);
		for(i = 1; i < n->count; i++) {
			com_apply_trailer(c, &n->children[i]);
		}
	}
}

static void com_term(c, n)struct compiling* c;
						  struct py_node* n;
{
	int i;
	int op;
	PY_REQ(n, term);
	com_factor(c, &n->children[0]);
	for(i = 2; i < n->count; i += 2) {
		com_factor(c, &n->children[i]);
		switch(n->children[i - 1].type) {
			case PY_STAR:op = PY_OP_BINARY_MULTIPLY;
				break;
			case PY_SLASH:op = PY_OP_BINARY_DIVIDE;
				break;
			case PY_PERCENT:op = PY_OP_BINARY_MODULO;
				break;
			default:
				py_error_set_string(
						py_system_error,
						"com_term: term operator not *, / or %");
				c->c_errors++;
				op = 255;
		}
		com_addbyte(c, op);
	}
}

static void com_expr(c, n)struct compiling* c;
						  struct py_node* n;
{
	int i;
	int op;
	PY_REQ(n, expr);
	com_term(c, &n->children[0]);
	for(i = 2; i < n->count; i += 2) {
		com_term(c, &n->children[i]);
		switch(n->children[i - 1].type) {
			case PY_PLUS:op = PY_OP_BINARY_ADD;
				break;
			case PY_MINUS:op = PY_OP_BINARY_SUBTRACT;
				break;
			default:
				py_error_set_string(
						py_system_error, "com_expr: expr operator not + or -");
				c->c_errors++;
				op = 255;
		}
		com_addbyte(c, op);
	}
}

static enum py_cmp_op cmp_type(n)struct py_node* n;
{
	PY_REQ(n, comp_op);
	/* comp_op: '<' | '>' | '=' | '>' '=' | '<' '=' | '<' '>'
			  | 'in' | 'not' 'in' | 'is' | 'is' not' */
	if(n->count == 1) {
		n = &n->children[0];
		switch(n->type) {
			case PY_LESS: return PY_CMP_LT;
			case PY_GREATER: return PY_CMP_GT;
			case PY_EQUAL: return PY_CMP_EQ;
			case PY_NAME: if(strcmp(n->str, "in") == 0) return PY_CMP_IN;
				if(strcmp(n->str, "is") == 0) return PY_CMP_IS;
		}
	}
	else if(n->count == 2) {
		int t2 = n->children[1].type;
		switch(n->children[0].type) {
			case PY_LESS: if(t2 == PY_EQUAL) return PY_CMP_LE;
				if(t2 == PY_GREATER) return PY_CMP_NE;
				break;
			case PY_GREATER: if(t2 == PY_EQUAL) return PY_CMP_GE;
				break;
			case PY_NAME:
				if(strcmp(n->children[1].str, "in") == 0) {
					return PY_CMP_NOT_IN;
				}
				if(strcmp(n->children[0].str, "is") == 0) {
					return PY_CMP_IS_NOT;
				}
		}
	}
	return PY_CMP_BAD;
}

static void com_comparison(c, n)struct compiling* c;
								struct py_node* n;
{
	int i;
	enum py_cmp_op op;
	int anchor;
	PY_REQ(n, comparison); /* comparison: expr (comp_op expr)* */
	com_expr(c, &n->children[0]);
	if(n->count == 1) {
		return;
	}

	/****************************************************************
	   The following code is generated for all but the last
	   comparison in a chain:

	   label:       on stack:       opcode:         jump to:

					a               <code to load b>
					a, b            PY_OP_DUP_TOP
					a, b, b         PY_OP_ROT_THREE
					b, a, b         PY_OP_COMPARE_OP
					b, 0-or-1       PY_OP_JUMP_IF_FALSE   L1
					b, 1            PY_OP_POP_TOP
					b

	   We are now ready to repeat this sequence for the next
	   comparison in the chain.

	   For the last we generate:

					b               <code to load c>
					b, c            PY_OP_COMPARE_OP
					0-or-1

	   If there were any jumps to L1 (i.e., there was more than one
	   comparison), we generate:

					0-or-1          PY_OP_JUMP_FORWARD    L2
	   L1:          b, 0            PY_OP_ROT_TWO
					0, b            PY_OP_POP_TOP
					0
	   L2:
	****************************************************************/

	anchor = 0;

	for(i = 2; i < n->count; i += 2) {
		com_expr(c, &n->children[i]);
		if(i + 2 < n->count) {
			com_addbyte(c, PY_OP_DUP_TOP);
			com_addbyte(c, PY_OP_ROT_THREE);
		}
		op = cmp_type(&n->children[i - 1]);
		if(op == PY_CMP_BAD) {
			py_error_set_string(
					py_system_error, "com_comparison: unknown comparison op");
			c->c_errors++;
		}
		com_addoparg(c, PY_OP_COMPARE_OP, op);
		if(i + 2 < n->count) {
			com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
			com_addbyte(c, PY_OP_POP_TOP);
		}
	}

	if(anchor) {
		int anchor2 = 0;
		com_addfwref(c, PY_OP_JUMP_FORWARD, &anchor2);
		com_backpatch(c, anchor);
		com_addbyte(c, PY_OP_ROT_TWO);
		com_addbyte(c, PY_OP_POP_TOP);
		com_backpatch(c, anchor2);
	}
}

static void com_not_test(c, n)struct compiling* c;
							  struct py_node* n;
{
	PY_REQ(n, not_test); /* 'not' not_test | comparison */
	if(n->count == 1) {
		com_comparison(c, &n->children[0]);
	}
	else {
		com_not_test(c, &n->children[1]);
		com_addbyte(c, PY_OP_UNARY_NOT);
	}
}

static void com_and_test(c, n)struct compiling* c;
							  struct py_node* n;
{
	int i;
	int anchor;
	PY_REQ(n, and_test); /* not_test ('and' not_test)* */
	anchor = 0;
	i = 0;
	for(;;) {
		com_not_test(c, &n->children[i]);
		if((i += 2) >= n->count) {
			break;
		}
		com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
		com_addbyte(c, PY_OP_POP_TOP);
	}
	if(anchor) {
		com_backpatch(c, anchor);
	}
}

static void com_test(c, n)struct compiling* c;
						  struct py_node* n;
{
	int i;
	int anchor;
	PY_REQ(n, test); /* and_test ('and' and_test)* */
	anchor = 0;
	i = 0;
	for(;;) {
		com_and_test(c, &n->children[i]);
		if((i += 2) >= n->count) {
			break;
		}
		com_addfwref(c, PY_OP_JUMP_IF_TRUE, &anchor);
		com_addbyte(c, PY_OP_POP_TOP);
	}
	if(anchor) {
		com_backpatch(c, anchor);
	}
}

static void com_list(c, n)struct compiling* c;
						  struct py_node* n;
{
	/* exprlist: expr (',' expr)* [',']; likewise for testlist */
	if(n->count == 1) {
		com_node(c, &n->children[0]);
	}
	else {
		int i;
		int len;
		len = (n->count + 1) / 2;
		for(i = 0; i < n->count; i += 2) {
			com_node(c, &n->children[i]);
		}
		com_addoparg(c, PY_OP_BUILD_TUPLE, len);
	}
}


/* Begin of assignment compilation */

static void com_assign_name(struct compiling*, struct py_node*, int);

static void com_assign(struct compiling*, struct py_node*, int);

static void com_assign_attr(c, n, assigning)struct compiling* c;
											struct py_node* n;
											int assigning;
{
	com_addopname(c, assigning ? PY_OP_STORE_ATTR : PY_OP_DELETE_ATTR, n);
}

static void com_assign_slice(c, n, assigning)struct compiling* c;
											 struct py_node* n;
											 int assigning;
{
	com_slice(c, n, assigning ? PY_OP_STORE_SLICE : PY_OP_DELETE_SLICE);
}

static void com_assign_subscript(c, n, assigning)struct compiling* c;
												 struct py_node* n;
												 int assigning;
{
	com_node(c, n);
	com_addbyte(c, assigning ? PY_OP_STORE_SUBSCR : PY_OP_DELETE_SUBSCR);
}

static void com_assign_trailer(c, n, assigning)struct compiling* c;
											   struct py_node* n;
											   int assigning;
{
	PY_REQ(n, trailer);
	switch(n->children[0].type) {
		case PY_LPAR: /* '(' [exprlist] ')' */
			py_error_set_string(py_type_error, "can't assign to function call");
			c->c_errors++;
			break;
		case PY_DOT: /* '.' PY_NAME */
			com_assign_attr(c, &n->children[1], assigning);
			break;
		case PY_LSQB: /* '[' subscript ']' */
			n = &n->children[1];
			PY_REQ(n, subscript); /* subscript: expr | [expr] ':' [expr] */
			if(n->count > 1 || n->children[0].type == PY_COLON) {
				com_assign_slice(c, n, assigning);
			}
			else {
				com_assign_subscript(c, &n->children[0], assigning);
			}
			break;
		default:py_error_set_string(py_type_error, "unknown trailer type");
			c->c_errors++;
	}
}

static void com_assign_tuple(c, n, assigning)struct compiling* c;
											 struct py_node* n;
											 int assigning;
{
	int i;
	if(n->type != testlist) PY_REQ(n, exprlist);
	if(assigning) {
		com_addoparg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);
	}
	for(i = 0; i < n->count; i += 2) {
		com_assign(c, &n->children[i], assigning);
	}
}

static void com_assign_list(c, n, assigning)struct compiling* c;
											struct py_node* n;
											int assigning;
{
	int i;
	if(assigning) {
		com_addoparg(c, PY_OP_UNPACK_LIST, (n->count + 1) / 2);
	}
	for(i = 0; i < n->count; i += 2) {
		com_assign(c, &n->children[i], assigning);
	}
}

static void com_assign_name(c, n, assigning)struct compiling* c;
											struct py_node* n;
											int assigning;
{
	PY_REQ(n, PY_NAME);
	com_addopname(c, assigning ? PY_OP_STORE_NAME : PY_OP_DELETE_NAME, n);
}

static void com_assign(c, n, assigning)struct compiling* c;
									   struct py_node* n;
									   int assigning;
{
	/* Loop to avoid trivial recursion */
	for(;;) {
		switch(n->type) {

			case exprlist:
			case testlist:
				if(n->count > 1) {
					com_assign_tuple(c, n, assigning);
					return;
				}
				n = &n->children[0];
				break;

			case test:
			case and_test:
			case not_test:
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					c->c_errors++;
					return;
				}
				n = &n->children[0];
				break;

			case comparison:
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					c->c_errors++;
					return;
				}
				n = &n->children[0];
				break;

			case expr:
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					c->c_errors++;
					return;
				}
				n = &n->children[0];
				break;

			case term:
				if(n->count > 1) {
					py_error_set_string(
							py_type_error, "can't assign to operator");
					c->c_errors++;
					return;
				}
				n = &n->children[0];
				break;

			case factor: /* ('+'|'-') factor | atom trailer* */
				if(n->children[0].type != atom) { /* '+' | '-' */
					py_error_set_string(
							py_type_error, "can't assign to operator");
					c->c_errors++;
					return;
				}
				if(n->count > 1) { /* trailer present */
					int i;
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

			case atom:
				switch(n->children[0].type) {
					case PY_LPAR:n = &n->children[1];
						if(n->type == PY_RPAR) {
							/* XXX Should allow () = () ??? */
							py_error_set_string(
									py_type_error, "can't assign to ()");
							c->c_errors++;
							return;
						}
						break;
					case PY_LSQB:n = &n->children[1];
						if(n->type == PY_RSQB) {
							py_error_set_string(
									py_type_error, "can't assign to []");
							c->c_errors++;
							return;
						}
						com_assign_list(c, n, assigning);
						return;
					case PY_NAME:com_assign_name(c, &n->children[0], assigning);
						return;
					default:
						py_error_set_string(
								py_type_error, "can't assign to constant");
						c->c_errors++;
						return;
				}
				break;

			default:fprintf(stderr, "node type %d\n", n->type);
				py_error_set_string(py_system_error, "com_assign: bad node");
				c->c_errors++;
				return;

		}
	}
}

static void com_expr_stmt(c, n)struct compiling* c;
							   struct py_node* n;
{
	PY_REQ(n, expr_stmt); /* exprlist ('=' exprlist)* PY_NEWLINE */
	com_node(c, &n->children[n->count - 2]);
	if(n->count == 2) {
		com_addbyte(c, PY_OP_PRINT_EXPR);
	}
	else {
		int i;
		for(i = 0; i < n->count - 3; i += 2) {
			if(i + 2 < n->count - 3) {
				com_addbyte(c, PY_OP_DUP_TOP);
			}
			com_assign(c, &n->children[i], 1/*assign*/);
		}
	}
}

static void com_print_stmt(c, n)struct compiling* c;
								struct py_node* n;
{
	int i;
	PY_REQ(n, print_stmt); /* 'print' (test ',')* [test] PY_NEWLINE */
	for(i = 1; i + 1 < n->count; i += 2) {
		com_node(c, &n->children[i]);
		com_addbyte(c, PY_OP_PRINT_ITEM);
	}
	if(n->children[n->count - 2].type != PY_COMMA) {
		com_addbyte(c, PY_OP_PRINT_NEWLINE);
	}
	/* XXX Alternatively, PY_OP_LOAD_CONST '\n' and then PY_OP_PRINT_ITEM */
}

static void com_return_stmt(c, n)struct compiling* c;
								 struct py_node* n;
{
	PY_REQ(n, return_stmt); /* 'return' [testlist] PY_NEWLINE */
	if(!c->c_infunction) {
		py_error_set_string(py_type_error, "'return' outside function");
		c->c_errors++;
	}
	if(n->count == 2) {
		com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	}
	else {
		com_node(c, &n->children[1]);
	}
	com_addbyte(c, PY_OP_RETURN_VALUE);
}

static void com_raise_stmt(c, n)struct compiling* c;
								struct py_node* n;
{
	PY_REQ(n, raise_stmt); /* 'raise' expr [',' expr] PY_NEWLINE */
	com_node(c, &n->children[1]);
	if(n->count > 3) {
		com_node(c, &n->children[3]);
	}
	else {
		com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	}
	com_addbyte(c, PY_OP_RAISE_EXCEPTION);
}

static void com_import_stmt(c, n)struct compiling* c;
								 struct py_node* n;
{
	int i;
	PY_REQ(n, import_stmt);
	/* 'import' PY_NAME (',' PY_NAME)* PY_NEWLINE |
	   'from' PY_NAME 'import' ('*' | PY_NAME (',' PY_NAME)*) PY_NEWLINE */
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

static void com_if_stmt(c, n)struct compiling* c;
							 struct py_node* n;
{
	int i;
	int anchor = 0;
	PY_REQ(n, if_stmt);
	/*'if' test ':' suite ('elif' test ':' suite)* ['else' ':' suite] */
	for(i = 0; i + 3 < n->count; i += 4) {
		int a = 0;
		struct py_node* ch = &n->children[i + 1];
		if(i > 0) {
			com_addoparg(c, PY_OP_SET_LINENO, ch->lineno);
		}
		com_node(c, &n->children[i + 1]);
		com_addfwref(c, PY_OP_JUMP_IF_FALSE, &a);
		com_addbyte(c, PY_OP_POP_TOP);
		com_node(c, &n->children[i + 3]);
		com_addfwref(c, PY_OP_JUMP_FORWARD, &anchor);
		com_backpatch(c, a);
		com_addbyte(c, PY_OP_POP_TOP);
	}
	if(i + 2 < n->count) {
		com_node(c, &n->children[i + 2]);
	}
	com_backpatch(c, anchor);
}

static void com_while_stmt(c, n)struct compiling* c;
								struct py_node* n;
{
	int break_anchor = 0;
	int anchor = 0;
	int begin;
	PY_REQ(n, while_stmt); /* 'while' test ':' suite ['else' ':' suite] */
	com_addfwref(c, PY_OP_SETUP_LOOP, &break_anchor);
	begin = c->c_nexti;
	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
	com_node(c, &n->children[1]);
	com_addfwref(c, PY_OP_JUMP_IF_FALSE, &anchor);
	com_addbyte(c, PY_OP_POP_TOP);
	c->c_loops++;
	com_node(c, &n->children[3]);
	c->c_loops--;
	com_addoparg(c, PY_OP_JUMP_ABSOLUTE, begin);
	com_backpatch(c, anchor);
	com_addbyte(c, PY_OP_POP_TOP);
	com_addbyte(c, PY_OP_POP_BLOCK);
	if(n->count > 4) {
		com_node(c, &n->children[6]);
	}
	com_backpatch(c, break_anchor);
}

static void com_for_stmt(c, n)struct compiling* c;
							  struct py_node* n;
{
	struct py_object* v;
	int break_anchor = 0;
	int anchor = 0;
	int begin;
	PY_REQ(n, for_stmt);
	/* 'for' exprlist 'in' exprlist ':' suite ['else' ':' suite] */
	com_addfwref(c, PY_OP_SETUP_LOOP, &break_anchor);
	com_node(c, &n->children[3]);
	v = py_int_new(0L);
	if(v == NULL) {
		c->c_errors++;
	}
	com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, v));
	py_object_decref(v);
	begin = c->c_nexti;
	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
	com_addfwref(c, PY_OP_FOR_LOOP, &anchor);
	com_assign(c, &n->children[1], 1/*assigning*/);
	c->c_loops++;
	com_node(c, &n->children[5]);
	c->c_loops--;
	com_addoparg(c, PY_OP_JUMP_ABSOLUTE, begin);
	com_backpatch(c, anchor);
	com_addbyte(c, PY_OP_POP_BLOCK);
	if(n->count > 8) {
		com_node(c, &n->children[8]);
	}
	com_backpatch(c, break_anchor);
}

/* Although 'execpt' and 'finally' clauses can be combined
   syntactically, they are compiled separately. In fact,
       try: S
       except E1: S1
       except E2: S2
       ...
       finally: Sf
   is equivalent to
       try:
           try: S
           except E1: S1
           except E2: S2
           ...
       finally: Sf
   meaning that the 'finally' clause is entered even if things
   go wrong again in an exception handler. Note that this is
   not the case for exception handlers: at most one is entered.

   Code generated for "try: S finally: Sf" is as follows:

               PY_OP_SETUP_FINALLY   L
               <code for S>
               PY_OP_POP_BLOCK
               PY_OP_LOAD_CONST      <nil>
       L:      <code for Sf>
               PY_OP_END_FINALLY

   The special instructions use the block stack. Each block
   stack entry contains the instruction that created it (here
   PY_OP_SETUP_FINALLY), the level of the value stack at the time the
   block stack entry was created, and a label (here L).

   PY_OP_SETUP_FINALLY:
       Pushes the current value stack level and the label
       onto the block stack.
   PY_OP_POP_BLOCK:
       Pops en entry from the block stack, and pops the value
       stack until its level is the same as indicated on the
       block stack. (The label is ignored.)
   PY_OP_END_FINALLY:
       Pops a variable number of entries from the *value* stack
       and re-raises the exception they specify. The number of
       entries popped depends on the (pseudo) exception type.

   The block stack is unwound when an exception is raised:
   when a PY_OP_SETUP_FINALLY entry is found, the exception is pushed
   onto the value stack (and the exception condition is cleared),
   and the interpreter jumps to the label gotten from the block
   stack.

   Code generated for "try: S except E1, V1: S1 except E2, V2: S2 ...":
   (The contents of the value stack is shown in [], with the top
   at the right; 'tb' is trace-back info, 'val' the exception's
   associated value, and 'exc' the exception.)

   Value stack         Label   Instruction     Argument
   []                          PY_OP_SETUP_EXCEPT    L1
   []                          <code for S>
   []                          PY_OP_POP_BLOCK
   []                          PY_OP_JUMP_FORWARD    L0

   [tb, val, exc]      L1:     DUP                             )
   [tb, val, exc, exc]         <evaluate E1>                     )
   [tb, val, exc, exc, E1]     PY_OP_COMPARE_OP      PY_CMP_EXC_MATCH       ) only if E1
   [tb, val, exc, 1-or-0]      PY_OP_JUMP_IF_FALSE   L2              )
   [tb, val, exc, 1]           POP                             )
   [tb, val, exc]              POP
   [tb, val]                   <assign to V1>    (or POP if no V1)
   [tb]                                POP
   []                          <code for S1>
                               PY_OP_JUMP_FORWARD    L0

   [tb, val, exc, 0]   L2:     POP
   [tb, val, exc]              DUP
   .............................etc.......................

   [tb, val, exc, 0]   Ln+1:   POP
   [tb, val, exc]              PY_OP_END_FINALLY     # re-raise exception

   []                  L0:     <next statement>

   Of course, parts are not generated if Vi or Ei is not present.
*/

static void com_try_stmt(c, n)struct compiling* c;
							  struct py_node* n;
{
	int finally_anchor = 0;
	int except_anchor = 0;
	PY_REQ(n, try_stmt);
	/* 'try' ':' suite (except_clause ':' suite)* ['finally' ':' suite] */

	if(n->count > 3 && n->children[n->count - 3].type != except_clause) {
		/* Have a 'finally' clause */
		com_addfwref(c, PY_OP_SETUP_FINALLY, &finally_anchor);
	}
	if(n->count > 3 && n->children[3].type == except_clause) {
		/* Have an 'except' clause */
		com_addfwref(c, PY_OP_SETUP_EXCEPT, &except_anchor);
	}
	com_node(c, &n->children[2]);
	if(except_anchor) {
		int end_anchor = 0;
		int i;
		struct py_node* ch;
		com_addbyte(c, PY_OP_POP_BLOCK);
		com_addfwref(c, PY_OP_JUMP_FORWARD, &end_anchor);
		com_backpatch(c, except_anchor);
		for(i = 3;
				i < n->count && (ch = &n->children[i])->type == except_clause;
				i += 3) {
			/* except_clause: 'except' [expr [',' expr]] */
			if(except_anchor == 0) {
				py_error_set_string(
						py_type_error, "default 'except:' must be last");
				c->c_errors++;
				break;
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
			if(ch->count > 3) {
				com_assign(c, &ch->children[3], 1/*assigning*/);
			}
			else {
				com_addbyte(c, PY_OP_POP_TOP);
			}
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

static void com_suite(c, n)struct compiling* c;
						   struct py_node* n;
{
	PY_REQ(n, suite);
	/* simple_stmt | PY_NEWLINE PY_INDENT PY_NEWLINE* (stmt PY_NEWLINE*)+ PY_DEDENT */
	if(n->count == 1) {
		com_node(c, &n->children[0]);
	}
	else {
		int i;
		for(i = 0; i < n->count; i++) {
			struct py_node* ch = &n->children[i];
			if(ch->type == stmt) {
				com_node(c, ch);
			}
		}
	}
}

static void com_funcdef(c, n)struct compiling* c;
							 struct py_node* n;
{
	struct py_object* v;
	PY_REQ(n, funcdef); /* funcdef: 'def' PY_NAME parameters ':' suite */
	v = (struct py_object*) py_compile(n, c->c_filename);
	if(v == NULL) {
		c->c_errors++;
	}
	else {
		int i = com_addconst(c, v);
		com_addoparg(c, PY_OP_LOAD_CONST, i);
		com_addbyte(c, PY_OP_BUILD_FUNCTION);
		com_addopname(c, PY_OP_STORE_NAME, &n->children[1]);
		py_object_decref(v);
	}
}

static void com_bases(c, n)struct compiling* c;
						   struct py_node* n;
{
	int i;
	PY_REQ(n, baselist);
	/*
	baselist: atom arguments (',' atom arguments)*
	arguments: '(' [testlist] ')'
	*/
	for(i = 0; i < n->count; i += 3) {
		com_node(c, &n->children[i]);
	}
	com_addoparg(c, PY_OP_BUILD_TUPLE, (n->count + 1) / 3);
}

static void com_classdef(c, n)struct compiling* c;
							  struct py_node* n;
{
	struct py_object* v;
	PY_REQ(n, classdef);
	/*
	classdef: 'class' PY_NAME parameters ['=' baselist] ':' suite
	baselist: atom arguments (',' atom arguments)*
	arguments: '(' [testlist] ')'
	*/
	if(n->count == 7) {
		com_bases(c, &n->children[4]);
	}
	else {
		com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	}
	v = (struct py_object*) py_compile(n, c->c_filename);
	if(v == NULL) {
		c->c_errors++;
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

static void com_node(c, n)struct compiling* c;
						  struct py_node* n;
{
	switch(n->type) {

		/* Definition nodes */

		case funcdef:com_funcdef(c, n);
			break;
		case classdef:com_classdef(c, n);
			break;

			/* Trivial parse tree nodes */

		case stmt:
		case flow_stmt:com_node(c, &n->children[0]);
			break;

		case simple_stmt:
		case compound_stmt:com_addoparg(c, PY_OP_SET_LINENO, n->lineno);
			com_node(c, &n->children[0]);
			break;

			/* Statement nodes */

		case expr_stmt:com_expr_stmt(c, n);
			break;
		case print_stmt:com_print_stmt(c, n);
			break;
		case del_stmt: /* 'del' exprlist PY_NEWLINE */
			com_assign(c, &n->children[1], 0/*delete*/);
			break;
		case pass_stmt:break;
		case break_stmt:
			if(c->c_loops == 0) {
				py_error_set_string(py_type_error, "'break' outside loop");
				c->c_errors++;
			}
			com_addbyte(c, PY_OP_BREAK_LOOP);
			break;
		case return_stmt:com_return_stmt(c, n);
			break;
		case raise_stmt:com_raise_stmt(c, n);
			break;
		case import_stmt:com_import_stmt(c, n);
			break;
		case if_stmt:com_if_stmt(c, n);
			break;
		case while_stmt:com_while_stmt(c, n);
			break;
		case for_stmt:com_for_stmt(c, n);
			break;
		case try_stmt:com_try_stmt(c, n);
			break;
		case suite:com_suite(c, n);
			break;

			/* Expression nodes */

		case testlist:com_list(c, n);
			break;
		case test:com_test(c, n);
			break;
		case and_test:com_and_test(c, n);
			break;
		case not_test:com_not_test(c, n);
			break;
		case comparison:com_comparison(c, n);
			break;
		case exprlist:com_list(c, n);
			break;
		case expr:com_expr(c, n);
			break;
		case term:com_term(c, n);
			break;
		case factor:com_factor(c, n);
			break;
		case atom:com_atom(c, n);
			break;

		default:fprintf(stderr, "node type %d\n", n->type);
			py_error_set_string(
					py_system_error, "com_node: unexpected node type");
			c->c_errors++;
	}
}

static void com_fplist(struct compiling*, struct py_node*);

static void com_fpdef(c, n)struct compiling* c;
						   struct py_node* n;
{
	PY_REQ(n, fpdef); /* fpdef: PY_NAME | '(' fplist ')' */
	if(n->children[0].type == PY_LPAR) {
		com_fplist(c, &n->children[1]);
	}
	else {
		com_addopname(c, PY_OP_STORE_NAME, &n->children[0]);
	}
}

static void com_fplist(c, n)struct compiling* c;
							struct py_node* n;
{
	PY_REQ(n, fplist); /* fplist: fpdef (',' fpdef)* */
	if(n->count == 1) {
		com_fpdef(c, &n->children[0]);
	}
	else {
		int i;
		com_addoparg(c, PY_OP_UNPACK_TUPLE, (n->count + 1) / 2);
		for(i = 0; i < n->count; i += 2) {
			com_fpdef(c, &n->children[i]);
		}
	}
}

static void com_file_input(c, n)struct compiling* c;
								struct py_node* n;
{
	int i;
	PY_REQ(n, file_input); /* (PY_NEWLINE | stmt)* PY_ENDMARKER */
	for(i = 0; i < n->count; i++) {
		struct py_node* ch = &n->children[i];
		if(ch->type != PY_ENDMARKER && ch->type != PY_NEWLINE) {
			com_node(c, ch);
		}
	}
}

/* Top-level py_compile-node interface */

static void compile_funcdef(c, n)struct compiling* c;
								 struct py_node* n;
{
	struct py_node* ch;
	PY_REQ(n, funcdef); /* funcdef: 'def' PY_NAME parameters ':' suite */
	ch = &n->children[2]; /* parameters: '(' [fplist] ')' */
	ch = &ch->children[1]; /* ')' | fplist */
	if(ch->type == PY_RPAR) {
		com_addbyte(c, PY_OP_REFUSE_ARGS);
	}
	else {
		com_addbyte(c, PY_OP_REQUIRE_ARGS);
		com_fplist(c, ch);
	}
	c->c_infunction = 1;
	com_node(c, &n->children[4]);
	c->c_infunction = 0;
	com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
	com_addbyte(c, PY_OP_RETURN_VALUE);
}

static void compile_node(c, n)struct compiling* c;
							  struct py_node* n;
{
	com_addoparg(c, PY_OP_SET_LINENO, n->lineno);

	switch(n->type) {

		case single_input: /* One interactive command */
			/* PY_NEWLINE | simple_stmt | compound_stmt PY_NEWLINE */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			n = &n->children[0];
			if(n->type != PY_NEWLINE) {
				com_node(c, n);
			}
			com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
			com_addbyte(c, PY_OP_RETURN_VALUE);
			break;

		case file_input: /* A whole file, or built-in function exec() */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_file_input(c, n);
			com_addoparg(c, PY_OP_LOAD_CONST, com_addconst(c, PY_NONE));
			com_addbyte(c, PY_OP_RETURN_VALUE);
			break;

		case expr_input: /* Built-in function eval() */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_node(c, &n->children[0]);
			com_addbyte(c, PY_OP_RETURN_VALUE);
			break;

		case eval_input: /* Built-in function input() */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_node(c, &n->children[0]);
			com_addbyte(c, PY_OP_RETURN_VALUE);
			break;

		case funcdef: /* A function definition */
			compile_funcdef(c, n);
			break;

		case classdef: /* A class definition */
			/* 'class' PY_NAME parameters ['=' baselist] ':' suite */
			com_addbyte(c, PY_OP_REFUSE_ARGS);
			com_node(c, &n->children[n->count - 1]);
			com_addbyte(c, PY_OP_LOAD_LOCALS);
			com_addbyte(c, PY_OP_RETURN_VALUE);
			break;

		default:fprintf(stderr, "node type %d\n", n->type);
			py_error_set_string(
					py_system_error, "compile_node: unexpected node type");
			c->c_errors++;
	}
}

struct py_code* py_compile(n, filename)struct py_node* n;
									   char* filename;
{
	struct compiling sc;
	struct py_code* co;
	if(!com_init(&sc, filename)) {
		return NULL;
	}
	compile_node(&sc, n);
	com_done(&sc);
	if(sc.c_errors == 0) {
		co = newcodeobject(sc.c_code, sc.c_consts, sc.c_names, filename);
	}
	else {
		co = NULL;
	}
	com_free(&sc);
	return co;
}
