/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser implementation */

/* For a description, see the comments at end of this file */

/* TODO: error recovery */

#include <python/std.h>
#include <python/token.h>
#include <python/grammar.h>
#include <python/node.h>
#include <python/parser.h>
#include <python/result.h>

struct py_dfa* py_grammar_find_dfa(struct py_grammar* g, int type) {
	unsigned i;
	struct py_dfa* d;

	for(i = 0, d = g->dfas; i < g->count; d++) {
		if(d->type == type) return d;
	}

	/* TODO: Better EH. */
	abort();
	/* NOTREACHED */
}

/* STACK DATA TYPE */

static void py_stack_reset(struct py_stack* s) {
	s->top = &s->base[PY_MAX_STACK];
}

#define py_stack_is_empty(s) ((s)->top == &(s)->base[PY_MAX_STACK])

static int py_stack_push(
		struct py_stack* s, struct py_dfa* d, struct py_node* parent) {

	struct py_stackentry* top;
	if(s->top == s->base) {
		fprintf(stderr, "py_stack_push: parser stack overflow\n");
		return -1;
	}

	top = --s->top;
	top->dfa = d;
	top->parent = parent;
	top->state = 0;

	return 0;
}

#ifdef _DEBUG
static void py_stack_pop(struct py_stack* s) {
	if(py_stack_is_empty(s)) {
		fprintf(stderr, "py_stack_pop: parser stack underflow -- FATAL\n");
		abort();
	}
	s->top++;
}
#else /* !DEBUG */
#define py_stack_pop(s) (s)->top++
#endif


/* PARSER CREATION */

struct py_parser* py_parser_new(struct py_grammar* g, int start) {
	struct py_parser* ps;

	if(!g->accel) {
		py_grammar_add_accels(g);
	}
	ps = malloc(sizeof(struct py_parser));
	if(ps == NULL) {
		return NULL;
	}
	ps->grammar = g;
	ps->tree = py_tree_new(start);
	if(ps->tree == NULL) {
		free(ps);
		return NULL;
	}
	py_stack_reset(&ps->stack);
	(void) py_stack_push(&ps->stack, py_grammar_find_dfa(g, start), ps->tree);
	return ps;
}

void py_parser_delete(struct py_parser* ps) {
	/*
	 * NB If you want to save the parse tree, you must set tree to NULL before
	 * calling py_parser_delete!
	 */
	py_tree_delete(ps->tree);
	free(ps);
}


/* PARSER STACK OPERATIONS */

static int py_parser_shift(
		struct py_stack* s, int type, char* str, int newstate, int lineno) {

	assert(!py_stack_is_empty(s));

	if(py_tree_add(s->top->parent, type, str, lineno) == NULL) {
		fprintf(stderr, "py_parser_shift: no mem in py_tree_add\n");
		return -1;
	}

	s->top->state = newstate;

	return 0;
}

static int py_parser_push(
		struct py_stack* s, int type, struct py_dfa* d, int newstate,
		int lineno) {

	struct py_node* n;

	n = s->top->parent;
	/* TODO: Better EH. */
	assert(!py_stack_is_empty(s));

	if(py_tree_add(n, type, NULL, lineno) == NULL) {
		fprintf(stderr, "py_parser_push: no mem in py_tree_add\n");
		return -1;
	}

	s->top->state = newstate;

	return py_stack_push(s, d, &n->children[n->count - 1]);
}


/* PARSER PROPER */

static int py_parser_classify(struct py_grammar* g, int type, char* str) {
	int n = g->labels.count;

	if(type == PY_NAME) {
		char* s = str;
		struct py_label* l = g->labels.label;
		int i;
		for(i = n; i > 0; i--, l++) {
			if(l->type == PY_NAME && l->str != NULL && l->str[0] == s[0] &&
				strcmp(l->str, s) == 0) {
				return n - i;
			}
		}
	}

	{
		struct py_label* l = g->labels.label;
		int i;
		for(i = n; i > 0; i--, l++) {
			if(l->type == type && l->str == NULL) return n - i;
		}
	}

	return -1;
}

enum py_result py_parser_add(
		struct py_parser* ps, int type, char* str, unsigned lineno) {

	int ilabel;

	/* Find out which label this token is */
	ilabel = py_parser_classify(ps->grammar, type, str);
	if(ilabel < 0) return PY_RESULT_SYNTAX;

	/* Loop until the token is py_parser_shifted or an error occurred */
	for(;;) {
		/* Fetch the current dfa and state */
		struct py_dfa* d = ps->stack.top->dfa;
		struct py_state* s = &d->states[ps->stack.top->state];

		/* Check accelerator */
		if(s->lower <= ilabel && ilabel < s->upper) {
			int x = s->accel[ilabel - s->lower];
			if(x != -1) {
				if(x & (1 << 7)) {
					/* Push non-terminal */
					int nt = (x >> 8) + PY_NONTERMINAL;
					int arrow = x & ((1 << 7) - 1);
					struct py_dfa* d1 = py_grammar_find_dfa(ps->grammar, nt);

					if(py_parser_push(
							&ps->stack, nt, d1, arrow, lineno) < 0) {

						return PY_RESULT_OOM;
					}

					continue;
				}

				/* Shift the token */
				if(py_parser_shift(&ps->stack, type, str, x, lineno) < 0) {
					return PY_RESULT_OOM;
				}

				/* Pop while we are in an accept-only state */
				while(s = &d->states[ps->stack.top->state], s->accept &&
															s->count == 1) {

					py_stack_pop(&ps->stack);
					if(py_stack_is_empty(&ps->stack)) return PY_RESULT_DONE;

					d = ps->stack.top->dfa;
				}

				return PY_RESULT_OK;
			}
		}

		if(s->accept) {
			/* Pop this dfa and try again */
			py_stack_pop(&ps->stack);
			if(py_stack_is_empty(&ps->stack)) return PY_RESULT_SYNTAX;

			continue;
		}

		/* Stuck, report syntax error */
		return PY_RESULT_SYNTAX;
	}
}

/*

Description
-----------

The parser's interface is different than usual: the function py_parser_add()
must be called for each token in the input. This makes it possible to
turn it into an incremental parsing system later. The parsing system
constructs a parse tree as it goes.

A parsing rule is represented as a Deterministic Finite-state Automaton
(DFA). A node in a DFA represents a state of the parser; an arc represents
a transition. Transitions are either labeled with terminal symbols or
with non-terminals. When the parser decides to follow an arc labeled
with a non-terminal, it is invoked recursively with the DFA representing
the parsing rule for that as its initial state; when that DFA accepts,
the parser that invoked it continues. The parse tree constructed by the
recursively called parser is inserted as a child in the current parse tree.

The DFA's can be constructed automatically from a more conventional
language description. An extended LL(1) grammar (ELL(1)) is suitable.
Certain restrictions make the parser's life easier: rules that can produce
the empty string should be outlawed (there are other ways to put loops
or optional parts in the language). To avoid the need to construct
FIRST sets, we can require that all but the last alternative of a rule
(really: arc going out of a DFA's state) must begin with a terminal
symbol.

As an example, consider this grammar:

PY_GRAMMAR_EXPRESSION:  PY_GRAMMAR_TERM (PY_OP PY_GRAMMAR_TERM)*
PY_GRAMMAR_TERM:  CONSTANT | '(' PY_GRAMMAR_EXPRESSION ')'

The DFA corresponding to the rule for PY_GRAMMAR_EXPRESSION is:

------->.---PY_GRAMMAR_TERM-->.------->
       ^          |
       |          |
       \----PY_OP----/

The parse tree generated for the input a+b is:

(PY_GRAMMAR_EXPRESSION: (PY_GRAMMAR_TERM: (PY_NAME: a)), (PY_OP: +), (PY_GRAMMAR_TERM: (PY_NAME: b)))

*/
