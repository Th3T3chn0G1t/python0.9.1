/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser generator */

/* For a description, see the comments at end of this file */

#include <python/token.h>
#include <python/errors.h>
#include <python/node.h>
#include <python/grammar.h>
#include <python/metagrammar.h>
#include <python/pgen.h>

extern int debugging;

/* PART ONE -- CONSTRUCT NFA -- Cf. Algorithm 3.2 from [Aho&Ullman 77] */

struct py_nfa_arc {
	unsigned label;
	unsigned arrow;
};

struct py_nfa_state {
	unsigned count;
	struct py_nfa_arc* arcs;
};

struct py_nfa {
	/* TODO: Signedness. */
	int type;
	char* name;

	unsigned count;
	struct py_nfa_state* states;

	int start, finish;
};

struct py_nfa_grammar {
	unsigned count;
	struct py_nfa** nfas;
	struct py_labellist labellist;
};

static int py_nfa_add_state(struct py_nfa* nf) {
	struct py_nfa_state* st;

	/* TODO: Leaky realloc. */
	nf->states = realloc(
			nf->states, (nf->count + 1) * sizeof(struct py_nfa_state));
	/* TODO: Better EH. */
	if(nf->states == NULL) py_fatal("out of mem");

	st = &nf->states[nf->count++];
	st->count = 0;
	st->arcs = NULL;

	return st - nf->states;
}

static void py_nfa_add_arc(
		struct py_nfa* nf, unsigned from, unsigned to, unsigned lbl) {

	struct py_nfa_state* st;
	struct py_nfa_arc* ar;

	st = &nf->states[from];

	/* TODO: Leaky realloc. */
	st->arcs = realloc(st->arcs, (st->count + 1) * sizeof(struct py_nfa_arc));
	/* TODO: Better EH. */
	if(st->arcs == NULL) py_fatal("out of mem");

	ar = &st->arcs[st->count++];
	ar->label = lbl;
	ar->arrow = to;
}

static struct py_nfa* py_nfa_new(char* name) {
	struct py_nfa* nf;
	static int type = PY_NONTERMINAL; /* All types will be disjunct */

	nf = malloc(sizeof(struct py_nfa));
	/* TODO: Better EH. */
	if(nf == NULL) py_fatal("no mem for new nfa");

	nf->type = type++;
	nf->name = name; /* TODO: strdup(name) ??? */
	nf->count = 0;
	nf->states = NULL;
	nf->start = nf->finish = -1;

	return nf;
}

static struct py_nfa_grammar* py_nfa_grammar_new(void) {
	struct py_nfa_grammar* gr;

	gr = malloc(sizeof(struct py_nfa_grammar));
	/* TODO: Better EH. */
	if(gr == NULL) py_fatal("no mem for new nfa grammar");

	gr->count = 0;
	gr->nfas = NULL;
	gr->labellist.count = 0;
	gr->labellist.label = NULL;
	py_labellist_add(&gr->labellist, PY_ENDMARKER, "EMPTY");

	return gr;
}

static struct py_nfa* addnfa(struct py_nfa_grammar* gr, char* name) {
	struct py_nfa* nf;

	nf = py_nfa_new(name);

	gr->nfas = realloc(gr->nfas, (gr->count + 1) * sizeof(struct py_nfa*));
	/* TODO: Better EH. */
	if(gr->nfas == NULL) py_fatal("out of mem");

	gr->nfas[gr->count++] = nf;
	py_labellist_add(&gr->labellist, PY_NAME, nf->name);

	return nf;
}

#ifdef _DEBUG
/* TODO: Better EH. */
#define PY_REQUIRE_N(i, count) \
       if (i < count) { \
		   fprintf(stderr, "py_node_compile_meta: less than %d children\n", count); \
		   abort(); \
       }

#else
#define PY_REQUIRE_N(i, count) (void) i, (void) count;
#endif

void py_node_compile_rule(struct py_nfa_grammar* gr, struct py_node* n);
void py_node_compile_rhs(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb);
void py_node_compile_alt(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb);
void py_node_compile_item(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb);
void compile_atom(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb);

/* TODO: Signedness. */
static struct py_nfa_grammar* py_node_compile_meta(struct py_node* n) {
	struct py_nfa_grammar* gr;
	int i;

	fprintf(stderr, "Compiling (meta-) parse tree into NFA grammar\n");
	gr = py_nfa_grammar_new();
	PY_REQ(n, PY_MSTART);
	i = (int) n->count - 1; /* Last child is PY_ENDMARKER */
	n = n->children;
	for(; --i >= 0; n++) {
		if(n->type != PY_NEWLINE) py_node_compile_rule(gr, n);
	}

	return gr;
}

void py_node_compile_rule(struct py_nfa_grammar* gr, struct py_node* n) {
	struct py_nfa* nf;

	PY_REQ(n, PY_RULE);
	PY_REQUIRE_N(n->count, 4);
	n = n->children;
	PY_REQ(n, PY_NAME);
	nf = addnfa(gr, n->str);
	n++;
	PY_REQ(n, PY_COLON);
	n++;
	PY_REQ(n, PY_RHS);
	py_node_compile_rhs(&gr->labellist, nf, n, &nf->start, &nf->finish);
	n++;
	PY_REQ(n, PY_NEWLINE);
}

void py_node_compile_rhs(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb) {

	int i;
	int a, b;

	PY_REQ(n, PY_RHS);
	i = n->count;
	PY_REQUIRE_N(i, 1);
	n = n->children;
	PY_REQ(n, PY_ALT);
	py_node_compile_alt(ll, nf, n, pa, pb);

	if(--i <= 0) return;

	n++;
	a = *pa;
	b = *pb;
	*pa = py_nfa_add_state(nf);
	*pb = py_nfa_add_state(nf);
	py_nfa_add_arc(nf, *pa, a, PY_LABEL_EMPTY);
	py_nfa_add_arc(nf, b, *pb, PY_LABEL_EMPTY);

	for(; --i >= 0; n++) {
		PY_REQ(n, PY_VBAR);
		PY_REQUIRE_N(i, 1);
		--i;
		n++;
		PY_REQ(n, PY_ALT);
		py_node_compile_alt(ll, nf, n, &a, &b);
		py_nfa_add_arc(nf, *pa, a, PY_LABEL_EMPTY);
		py_nfa_add_arc(nf, b, *pb, PY_LABEL_EMPTY);
	}
}

void py_node_compile_alt(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb) {

	int i;
	int a, b;

	PY_REQ(n, PY_ALT);
	i = n->count;
	PY_REQUIRE_N(i, 1);
	n = n->children;
	PY_REQ(n, PY_ITEM);
	py_node_compile_item(ll, nf, n, pa, pb);
	--i;
	n++;

	for(; --i >= 0; n++) {
		if(n->type == PY_COMMA) { /* TODO: Temporary */
			PY_REQUIRE_N(i, 1);
			--i;
			n++;
		}
		PY_REQ(n, PY_ITEM);
		py_node_compile_item(ll, nf, n, &a, &b);
		py_nfa_add_arc(nf, *pb, a, PY_LABEL_EMPTY);
		*pb = b;
	}
}

void py_node_compile_item(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa,
		int* pb) {

	int i;
	int a, b;

	PY_REQ(n, PY_ITEM);
	i = n->count;
	PY_REQUIRE_N(i, 1);
	n = n->children;
	if(n->type == PY_LSQB) {
		PY_REQUIRE_N(i, 3);
		n++;
		PY_REQ(n, PY_RHS);
		*pa = py_nfa_add_state(nf);
		*pb = py_nfa_add_state(nf);
		py_nfa_add_arc(nf, *pa, *pb, PY_LABEL_EMPTY);
		py_node_compile_rhs(ll, nf, n, &a, &b);
		py_nfa_add_arc(nf, *pa, a, PY_LABEL_EMPTY);
		py_nfa_add_arc(nf, b, *pb, PY_LABEL_EMPTY);
		PY_REQUIRE_N(i, 1);
		n++;
		PY_REQ(n, PY_RSQB);
	}
	else {
		compile_atom(ll, nf, n, pa, pb);

		if(--i <= 0) return;

		n++;
		py_nfa_add_arc(nf, *pb, *pa, PY_LABEL_EMPTY);

		if(n->type == PY_STAR) *pb = *pa;
		else PY_REQ(n, PY_PLUS);
	}
}

void compile_atom(
		struct py_labellist* ll, struct py_nfa* nf, struct py_node* n, int* pa, int* pb) {

	int i;

	PY_REQ(n, PY_ATOM);
	i = n->count;
	PY_REQUIRE_N(i, 1);
	n = n->children;
	if(n->type == PY_LPAR) {
		PY_REQUIRE_N(i, 3);
		n++;
		PY_REQ(n, PY_RHS);
		py_node_compile_rhs(ll, nf, n, pa, pb);
		n++;
		PY_REQ(n, PY_RPAR);
	}
	else if(n->type == PY_NAME || n->type == PY_STRING) {
		*pa = py_nfa_add_state(nf);
		*pb = py_nfa_add_state(nf);
		py_nfa_add_arc(nf, *pa, *pb, py_labellist_add(ll, n->type, n->str));
	}
	else
		PY_REQ(n, PY_NAME);
}

static void py_nfa_dump_state(
		struct py_labellist* ll, struct py_nfa* nf, int istate) {

	struct py_nfa_state* st;
	struct py_nfa_arc* ar;
	unsigned i;

	printf(
			"%c%2d%c", istate == nf->start ? '*' : ' ', istate,
			istate == nf->finish ? '.' : ' ');

	st = &nf->states[istate];
	ar = st->arcs;

	for(i = 0; i < st->count; i++) {
		if(i > 0) printf("\n    ");

		printf(
				"-> %2d  %s", ar->arrow,
				py_label_repr(&ll->label[ar->label]));
		ar++;
	}

	printf("\n");
}

static void py_nfa_dump(struct py_labellist* ll, struct py_nfa* nf) {
	unsigned i;

	printf(
			"NFA '%s' has %d states; start %d, finish %d\n", nf->name,
			nf->count, nf->start, nf->finish);

	for(i = 0; i < nf->count; i++) py_nfa_dump_state(ll, nf, i);
}


/* PART TWO -- CONSTRUCT DFA -- Algorithm 3.1 from [Aho&Ullman 77] */

static void py_nfa_add_closure(py_bitset_t ss, struct py_nfa* nf, int istate) {
	if(py_bitset_add(ss, istate)) {
		struct py_nfa_state* st = &nf->states[istate];
		struct py_nfa_arc* ar = st->arcs;
		int i;

		for(i = st->count; --i >= 0;) {
			if(ar->label == PY_LABEL_EMPTY) {
				py_nfa_add_closure(ss, nf, ar->arrow);
			}

			ar++;
		}
	}
}

/* TODO: Figure out what `ss' means and name these better. */

struct py_ss_arc {
	py_bitset_t bitset;
	unsigned arrow;
	unsigned label;
};

struct py_ss_state {
	py_bitset_t bitset;

	unsigned count;
	struct py_ss_arc* arcs;

	int deleted;
	int finish;
	int rename;
};

void py_ss_dfa_print(
		unsigned, struct py_ss_state*, unsigned, struct py_labellist*, char*);

void py_ss_state_simplify(unsigned, struct py_ss_state*);

void py_dfa_convert(struct py_dfa*, unsigned, struct py_ss_state*);

static void py_dfa_new(
		struct py_nfa_grammar* gr, struct py_nfa* nf, struct py_dfa* d) {

	unsigned nbits = nf->count;
	py_bitset_t ss;
	unsigned nstates;
	struct py_ss_state* states;
	struct py_ss_state* current;
	struct py_ss_arc* ss_arc;
	unsigned istate, jstate, iarc, jarc, ibit;
	struct py_nfa_state* st;
	struct py_nfa_arc* ar;

	ss = py_bitset_new(nbits);
	py_nfa_add_closure(ss, nf, nf->start);
	states = malloc(sizeof(struct py_ss_state));
	if(states == NULL) py_fatal("no mem for state in py_dfa_new");

	nstates = 1;
	current = &states[0];
	current->bitset = ss;
	current->count = 0;
	current->arcs = NULL;
	current->deleted = 0;
	current->finish = PY_TESTBIT(ss, nf->finish);

	if(current->finish) {
		printf(
				"Error: nonterminal '%s' may produce empty.\n", nf->name);
	}

	/* This algorithm is from a book written before
	   the invention of structured programming... */

	/* For each unmarked state... */
	for(istate = 0; istate < nstates; ++istate) {
		current = &states[istate];
		ss = current->bitset;
		/* For all its states... */
		for(ibit = 0; ibit < nf->count; ++ibit) {
			if(!PY_TESTBIT(ss, ibit)) {
				continue;
			}
			st = &nf->states[ibit];
			/* For all non-empty arcs from this state... */
			for(iarc = 0; iarc < st->count; iarc++) {
				ar = &st->arcs[iarc];
				if(ar->label == PY_LABEL_EMPTY) {
					continue;
				}
				/* Look up in list of arcs from this state */
				for(jarc = 0; jarc < current->count; ++jarc) {
					ss_arc = &current->arcs[jarc];
					if(ar->label == ss_arc->label) {
						goto found;
					}
				}
				/* Add new arc for this state */
				/* TODO: Leaky realloc. */
				current->arcs = realloc(
						current->arcs, (current->count + 1) * sizeof(struct py_ss_arc));
				if(current->arcs == NULL) {
					py_fatal("out of mem");
				}
				ss_arc = &current->arcs[current->count++];
				ss_arc->label = ar->label;
				ss_arc->bitset = py_bitset_new(nbits);
				ss_arc->arrow = UINT_MAX;

				found:;
				/* Add destination */
				py_nfa_add_closure(ss_arc->bitset, nf, ar->arrow);
			}
		}

		/* Now look up all the arrow states */
		for(jarc = 0; jarc < states[istate].count; jarc++) {
			ss_arc = &states[istate].arcs[jarc];

			for(jstate = 0; jstate < nstates; jstate++) {
				if(py_bitset_cmp(
						ss_arc->bitset, states[jstate].bitset, nbits)) {

					ss_arc->arrow = jstate;
					goto done;
				}
			}

			/* TODO: Leaky realloc. */
			states = realloc(states, (nstates + 1) * sizeof(struct py_ss_state));
			if(states == NULL) py_fatal("out of mem");

			ss_arc->arrow = nstates;
			current = &states[nstates++];
			current->bitset = ss_arc->bitset;
			current->count = 0;
			current->arcs = NULL;
			current->deleted = 0;
			current->finish = PY_TESTBIT(current->bitset, nf->finish);
			done:;
		}
	}

	if(debugging) {
		py_ss_dfa_print(
				nstates, states, nbits, &gr->labellist, "before minimizing");
	}

	py_ss_state_simplify(nstates, states);

	if(debugging) {
		py_ss_dfa_print(
				nstates, states, nbits, &gr->labellist, "after minimizing");
	}

	py_dfa_convert(d, nstates, states);

	/* TODO: cleanup */
}

void py_ss_dfa_print(
		unsigned nstates, struct py_ss_state* state, unsigned nbits,
		struct py_labellist* ll, char* msg) {

	unsigned i, ibit, iarc;
	struct py_ss_state* current;
	struct py_ss_arc* ss_arc;

	printf("Subset DFA %s\n", msg);
	for(i = 0; i < nstates; i++) {
		current = &state[i];
		if(current->deleted) {
			continue;
		}
		printf(" Subset %d", i);
		if(current->finish) {
			printf(" (finish)");
		}
		printf(" { ");
		for(ibit = 0; ibit < nbits; ibit++) {
			if(PY_TESTBIT(current->bitset, ibit)) {
				printf("%d ", ibit);
			}
		}
		printf("}\n");
		for(iarc = 0; iarc < current->count; iarc++) {
			ss_arc = &current->arcs[iarc];
			printf(
					"  Arc to state %d, label %s\n", ss_arc->arrow,
					py_label_repr(&ll->label[ss_arc->label]));
		}
	}
}


/* PART THREE -- SIMPLIFY DFA */

/*
 * Simplify the DFA by repeatedly eliminating states that are
 * equivalent to another one. This is NOT Algorithm 3.3 from
 * [Aho&Ullman 77]. It does not always finds the minimal DFA,
 * but it does usually make a much smaller one...  (For an example
 * of sub-optimal behaviour, try S: x a b+ | y a b+.)
 */

static int py_ss_state_cmp(struct py_ss_state* s1, struct py_ss_state* s2) {
	unsigned i;

	if(s1->count != s2->count || s1->finish != s2->finish) {
		return 0;
	}
	for(i = 0; i < s1->count; i++) {
		if(s1->arcs[i].arrow != s2->arcs[i].arrow ||
		   s1->arcs[i].label != s2->arcs[i].label) {
			return 0;
		}
	}

	return 1;
}

static void py_ss_state_rename(
		unsigned nstates, struct py_ss_state* state, unsigned from,
		unsigned to) {

	unsigned i, j;

	if(debugging) printf("Rename state %d to %d.\n", from, to);

	for(i = 0; i < nstates; i++) {
		if(state[i].deleted) {
			continue;
		}
		for(j = 0; j < state[i].count; j++) {
			if(state[i].arcs[j].arrow == from) {
				state[i].arcs[j].arrow = to;
			}
		}
	}
}

void py_ss_state_simplify(unsigned nstates, struct py_ss_state* state) {
	int changes;
	unsigned i, j;

	do {
		changes = 0;
		for(i = 1; i < nstates; i++) {
			if(state[i].deleted) continue;

			for(j = 0; j < i; j++) {
				if(state[j].deleted) {
					continue;
				}
				if(py_ss_state_cmp(&state[i], &state[j])) {
					state[i].deleted++;
					py_ss_state_rename(nstates, state, i, j);
					changes++;
					break;
				}
			}
		}
	} while(changes);
}


/* PART FOUR -- GENERATE PARSING TABLES */

/* Convert the DFA into a grammar that can be used by our parser */

void py_dfa_convert(
		struct py_dfa* d, unsigned nstates, struct py_ss_state* state) {

	unsigned i, j;
	struct py_ss_state* current;
	struct py_ss_arc* ss_arc;

	for(i = 0; i < nstates; i++) {
		current = &state[i];

		if(current->deleted) continue;

		current->rename = py_dfa_add_state(d);
	}

	for(i = 0; i < nstates; i++) {
		current = &state[i];

		if(current->deleted) continue;

		for(j = 0; j < current->count; j++) {
			ss_arc = &current->arcs[j];

			py_dfa_add_arc(
					d, current->rename, state[ss_arc->arrow].rename,
					ss_arc->label);
		}

		if(current->finish) {
			py_dfa_add_arc(d, current->rename, current->rename, 0);
		}
	}

	d->initial = 0;
}


/* PART FIVE -- GLUE IT ALL TOGETHER */

static struct py_grammar* py_nfa_grammar_tables(struct py_nfa_grammar* gr) {
	unsigned i;
	struct py_nfa* nf;
	struct py_dfa* d;
	struct py_grammar* g;

	if(gr->count == 0) return NULL;

	g = py_grammar_new(gr->nfas[0]->type);
	/* TODO: first rule must be start rule */
	g->labels = gr->labellist;

	for(i = 0; i < gr->count; i++) {
		nf = gr->nfas[i];

		if(debugging) {
			printf("Dump of NFA for '%s' ...\n", nf->name);
			py_nfa_dump(&gr->labellist, nf);
		}

		fprintf(stderr, "Making DFA for '%s' ...\n", nf->name);
		d = py_grammar_add_dfa(g, nf->type, nf->name);
		py_dfa_new(gr, gr->nfas[i], d);
	}

	return g;
}

struct py_grammar* py_grammar_gen(struct py_node* n) {
	struct py_nfa_grammar* gr;
	struct py_grammar* g;

	gr = py_node_compile_meta(n);
	g = py_nfa_grammar_tables(gr);
	py_grammar_translate(g);
	py_grammar_add_firsts(g);

	return g;
}


/*
Description
-----------

Input is a grammar in extended BNF (using * for repetition, + for
at-least-once repetition, [] for optional parts, | for alternatives and
() for grouping). This has already been parsed and turned into a parse
tree.

Each rule is considered as a regular expression in its own right.
It is turned into a Non-deterministic Finite Automaton (NFA), which
is then turned into a Deterministic Finite Automaton (DFA), which is then
optimized to reduce the number of states. See [Aho&Ullman 77] chapter 3,
or similar compiler books (this technique is more often used for lexical
analyzers).

The DFA's are used by the parser as parsing tables in a special way
that's probably unique. Before they are usable, the FIRST sets of all
non-terminals are computed.

Reference
---------

[Aho&Ullman 77]
       Aho&Ullman, Principles of Compiler Design, Addison-Wesley 1977
       (first edition)
*/
