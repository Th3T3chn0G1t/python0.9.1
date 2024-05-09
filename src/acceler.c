/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser accelerator module */

/* The parser as originally conceived had disappointing performance.
   This module does some precomputation that speeds up the selection
   of a DFA based upon a token, turning a search through an array
   into a simple indexing operation. The parser now cannot work
   without the accelerators installed. Note that the accelerators
   are installed dynamically when the parser is initialized, they
   are not part of the static data structure written on graminit.[ch]
   by the parser generator. */

#include <python/std.h>
#include <python/grammar.h>
#include <python/token.h>
#include <python/parser.h>

/* TODO: This isn't a great system. */
static int** freelist = 0;
static int freelist_len = 0;

/* Forward references */
static void fixdfa(struct py_grammar*, struct py_dfa*);

static void fixstate(struct py_grammar*, struct py_state*);

void py_grammar_add_accels(struct py_grammar* g) {
	struct py_dfa* d;
	int i;

	d = g->dfas;
	for(i = g->count; --i >= 0; d++) fixdfa(g, d);
	g->accel = 1;
}

static void fixdfa(struct py_grammar* g, struct py_dfa* d) {
	struct py_state* s;
	unsigned j;

	s = d->states;
	for(j = 0; j < d->count; j++, s++) fixstate(g, s);
}

void py_grammar_delete_accels(void) {
	int i;
	for(i = 0; i < freelist_len; ++i) free(freelist[i]);
	free(freelist);
}

static void fixstate(struct py_grammar* g, struct py_state* s) {
	struct py_arc* a;
	int k;
	int* accel;
	int nl = g->labels.count;

	s->accept = 0;
	accel = malloc(nl * sizeof(int));

	for(k = 0; k < nl; k++) accel[k] = -1;
	a = s->arcs;

	for(k = s->count; --k >= 0; a++) {
		int lbl = a->label;
		struct py_label* l = &g->labels.label[lbl];
		int type = l->type;

		if(a->arrow >= (1 << 7)) {
			printf("XXX too many states!\n");
			continue;
		}

		if(type >= PY_NONTERMINAL) {
			struct py_dfa* d1 = py_grammar_find_dfa(g, type);
			unsigned ibit;

			if(type - PY_NONTERMINAL >= (1 << 7)) {
				printf("XXX too high nonterminal number!\n");
				continue;
			}

			for(ibit = 0; ibit < g->labels.count; ibit++) {
				if(PY_TESTBIT(d1->first, ibit)) {
					if(accel[ibit] != -1) printf("XXX ambiguity!\n");

					accel[ibit] = a->arrow | (1 << 7);
					accel[ibit] |= ((type - PY_NONTERMINAL) << 8);
				}
			}
		}
		else if(lbl == PY_LABEL_EMPTY) { s->accept = 1; }
		else if(lbl >= 0 && lbl < nl) accel[lbl] = a->arrow;
	}

	while(nl > 0 && accel[nl - 1] == -1) nl--;
	for(k = 0; k < nl && accel[k] == -1;) k++;

	if(k < nl) {
		int i;
		void* newptr;

		/* TODO: Better EH. */
		if(!(s->accel = malloc((nl - k) * sizeof(int)))) {
			fprintf(stderr, "no mem to add parser accelerators\n");
			exit(1);
		}

		if(!(newptr = realloc(freelist, ++freelist_len * sizeof(int*)))) {
			free(freelist);
			fprintf(stderr, "no mem to add parser accelerators\n");
			exit(1);
		}
		freelist = newptr;

		freelist[freelist_len - 1] = s->accel;
		s->lower = k;
		s->upper = nl;

		for(i = 0; k < nl; i++, k++) s->accel[i] = accel[k];
	}

	free(accel);
}
