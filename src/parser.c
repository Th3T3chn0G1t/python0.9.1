/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser implementation */

/* For a description, see the comments at end of this file */

/* XXX To do: error recovery */

#include <stdlib.h>
#include <assert.h>

#include <python/token.h>
#include <python/grammar.h>
#include <python/node.h>
#include <python/parser.h>
#include <python/result.h>

#ifdef _DEBUG
extern int debugging;
#define D(x) if (!debugging); else x
#else
#define D(x)
#endif


/* STACK DATA TYPE */

static void s_reset(s)struct py_stack* s;
{
	s->top = &s->base[PY_MAX_STACK];
}

#define s_empty(s) ((s)->top == &(s)->base[PY_MAX_STACK])

static int s_push(s, d, parent)struct py_stack* s;
							   struct py_dfa* d;
							   struct py_node* parent;
{
	struct py_stackentry* top;
	if(s->top == s->base) {
		fprintf(stderr, "s_push: parser stack overflow\n");
		return -1;
	}
	top = --s->top;
	top->dfa = d;
	top->parent = parent;
	top->state = 0;
	return 0;
}

#ifdef _DEBUG

static void s_pop(s)struct py_stack* s;
{
	if(s_empty(s)) {
		fprintf(stderr, "s_pop: parser stack underflow -- FATAL\n");
		abort();
	}
	s->top++;
}

#else /* !DEBUG */

#define s_pop(s) (s)->top++

#endif


/* PARSER CREATION */

struct py_parser* py_parser_new(g, start)struct py_grammar* g;
										 int start;
{
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
	s_reset(&ps->stack);
	(void) s_push(&ps->stack, py_grammar_find_dfa(g, start), ps->tree);
	return ps;
}

void py_parser_delete(ps)struct py_parser* ps;
{
	/* NB If you want to save the parse tree,
	   you must set tree to NULL before calling py_parser_delete! */
	py_tree_delete(ps->tree);
	free(ps);
}


/* PARSER STACK OPERATIONS */

static int shift(s, type, str, newstate, lineno)struct py_stack* s;
												int type;
												char* str;
												int newstate;
												int lineno;
{
	assert(!s_empty(s));
	if(py_tree_add(s->top->parent, type, str, lineno) == NULL) {
		fprintf(stderr, "shift: no mem in py_tree_add\n");
		return -1;
	}
	s->top->state = newstate;
	return 0;
}

static int push(s, type, d, newstate, lineno)struct py_stack* s;
											 int type;
											 struct py_dfa* d;
											 int newstate;
											 int lineno;
{
	struct py_node* n;
	n = s->top->parent;
	assert(!s_empty(s));
	if(py_tree_add(n, type, NULL, lineno) == NULL) {
		fprintf(stderr, "push: no mem in py_tree_add\n");
		return -1;
	}
	s->top->state = newstate;
	return s_push(s, d, &n->children[n->count - 1]);
}


/* PARSER PROPER */

static int classify(g, type, str)struct py_grammar* g;
								 int type;
								 char* str;
{
	int n = g->labels.count;

	if(type == PY_NAME) {
		char* s = str;
		struct py_label* l = g->labels.label;
		int i;
		for(i = n; i > 0; i--, l++) {
			if(l->type == PY_NAME && l->str != NULL && l->str[0] == s[0] &&
			   strcmp(l->str, s) == 0) {
				D(printf("It's a keyword\n"));
				return n - i;
			}
		}
	}

	{
		struct py_label* l = g->labels.label;
		int i;
		for(i = n; i > 0; i--, l++) {
			if(l->type == type && l->str == NULL) {
				D(printf("It's a token we know\n"));
				return n - i;
			}
		}
	}

	D(printf("Illegal token\n"));
	return -1;
}

int py_parser_add(ps, type, str, lineno)struct py_parser* ps;
										int type;
										char* str;
										int lineno;
{
	int ilabel;

	D(printf("Token %s/'%s' ... ", py_token_names[type], str));

	/* Find out which label this token is */
	ilabel = classify(ps->grammar, type, str);
	if(ilabel < 0) {
		return PY_RESULT_SYNTAX;
	}

	/* Loop until the token is shifted or an error occurred */
	for(;;) {
		/* Fetch the current dfa and state */
		struct py_dfa* d = ps->stack.top->dfa;
		struct py_state* s = &d->states[ps->stack.top->state];

		D(printf(
				" DFA '%s', state %d:", d->name, ps->stack.top->state));

		/* Check accelerator */
		if(s->lower <= ilabel && ilabel < s->upper) {
			int x = s->accel[ilabel - s->lower];
			if(x != -1) {
				if(x & (1 << 7)) {
					/* Push non-terminal */
					int nt = (x >> 8) + PY_NONTERMINAL;
					int arrow = x & ((1 << 7) - 1);
					struct py_dfa* d1 = py_grammar_find_dfa(ps->grammar, nt);
					if(push(
							&ps->stack, nt, d1, arrow, lineno) < 0) {
						D(printf(" MemError: push.\n"));
						return PY_RESULT_OOM;
					}
					D(printf(" Push ...\n"));
					continue;
				}

				/* Shift the token */
				if(shift(
						&ps->stack, type, str, x, lineno) < 0) {
					D(printf(" MemError: shift.\n"));
					return PY_RESULT_OOM;
				}
				D(printf(" Shift.\n"));
				/* Pop while we are in an accept-only state */
				while(s = &d->states[ps->stack.top->state], s->accept &&
															s->count == 1) {
					D(printf("  Direct pop.\n"));
					s_pop(&ps->stack);
					if(s_empty(&ps->stack)) {
						D(printf("  ACCEPT.\n"));
						return PY_RESULT_DONE;
					}
					d = ps->stack.top->dfa;
				}
				return PY_RESULT_OK;
			}
		}

		if(s->accept) {
			/* Pop this dfa and try again */
			s_pop(&ps->stack);
			D(printf(" Pop ...\n"));
			if(s_empty(&ps->stack)) {
				D(printf(" Error: bottom of stack.\n"));
				return PY_RESULT_SYNTAX;
			}
			continue;
		}

		/* Stuck, report syntax error */
		D(printf(" Error.\n"));
		return PY_RESULT_SYNTAX;
	}
}


#ifdef _DEBUG

/* DEBUG OUTPUT */

void dumptree(g, n)struct py_grammar* g;
				   struct py_node* n;
{
	unsigned i;

	if(n == NULL) {
		printf("NIL");
	}
	else {
		struct py_label l;
		l.type = n->type;
		l.str = n->str;
		printf("%s", py_label_repr(&l));
		if(n->type >= PY_NONTERMINAL) {
			printf("(");
			for(i = 0; i < n->count; i++) {
				if(i > 0) {
					printf(",");
				}
				dumptree(g, &n->children[i]);
			}
			printf(")");
		}
	}
}

void showtree(g, n)struct py_grammar* g;
				   struct py_node* n;
{
	unsigned i;

	if(n == NULL) {
		return;
	}
	if(n->type >= PY_NONTERMINAL) {
		for(i = 0; i < n->count; i++) {
			showtree(g, &n->children[i]);
		}
	}
	else if(n->type < PY_NONTERMINAL) {
		printf("%s", py_token_names[n->type]);
		if(n->type == PY_NUMBER || n->type == PY_NAME) {
			printf("(%s)", n->str);
		}
		printf(" ");
	}
	else {
		printf("? ");
	}
}

void printtree(ps)struct py_parser* ps;
{
	if(debugging) {
		printf("Parse tree:\n");
		dumptree(ps->grammar, ps->tree);
		printf("\n");
		printf("Tokens:\n");
		showtree(ps->grammar, ps->tree);
		printf("\n");
	}
	printf("Listing:\n");
	py_tree_list(stdout, ps->tree);
	printf("\n");
}

#endif /* DEBUG */

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

expr:  term (PY_OP term)*
term:  CONSTANT | '(' expr ')'

The DFA corresponding to the rule for expr is:

------->.---term-->.------->
       ^          |
       |          |
       \----PY_OP----/

The parse tree generated for the input a+b is:

(expr: (term: (PY_NAME: a)), (PY_OP: +), (term: (PY_NAME: b)))

*/
