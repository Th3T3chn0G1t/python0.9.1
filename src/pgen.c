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
	nf->name = name; /* XXX strdup(name) ??? */
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
#define PY_REQUIRE_N(i, count) /* empty */
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
		if(n->type == PY_COMMA) { /* XXX Temporary */
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

static void dumpstate(ll, nf, istate)struct py_labellist* ll;
									 struct py_nfa* nf;
									 int istate;
{
	struct py_nfa_state* st;
	struct py_nfa_arc* ar;
	unsigned i;

	printf(
			"%c%2d%c", istate == nf->start ? '*' : ' ', istate,
			istate == nf->finish ? '.' : ' ');
	st = &nf->states[istate];
	ar = st->arcs;
	for(i = 0; i < st->count; i++) {
		if(i > 0) {
			printf("\n    ");
		}
		printf(
				"-> %2d  %s", ar->arrow,
				py_label_repr(&ll->label[ar->label]));
		ar++;
	}
	printf("\n");
}

static void dumpnfa(ll, nf)struct py_labellist* ll;
						   struct py_nfa* nf;
{
	unsigned i;

	printf(
			"NFA '%s' has %d states; start %d, finish %d\n", nf->name,
			nf->count, nf->start, nf->finish);

	for(i = 0; i < nf->count; i++) dumpstate(ll, nf, i);
}


/* PART TWO -- CONSTRUCT DFA -- Algorithm 3.1 from [Aho&Ullman 77] */

static void addclosure(ss, nf, istate)py_bitset_t ss;
									  struct py_nfa* nf;
									  int istate;
{
	if(py_bitset_add(ss, istate)) {
		struct py_nfa_state* st = &nf->states[istate];
		struct py_nfa_arc* ar = st->arcs;
		int i;

		for(i = st->count; --i >= 0;) {
			if(ar->label == PY_LABEL_EMPTY) {
				addclosure(ss, nf, ar->arrow);
			}
			ar++;
		}
	}
}

typedef struct _ss_arc {
	py_bitset_t sa_bitset;
	unsigned sa_arrow;
	unsigned sa_label;
} ss_arc;

typedef struct _ss_state {
	py_bitset_t ss_ss;
	unsigned ss_narcs;
	ss_arc* ss_arc;
	int ss_deleted;
	int ss_finish;
	int ss_rename;
} ss_state;

typedef struct _ss_dfa {
	int sd_nstates;
	ss_state* sd_state;
} ss_dfa;

void printssdfa(
		unsigned xx_nstates, ss_state* xx_state, unsigned nbits,
		struct py_labellist* ll, char* msg);

void simplify(int xx_nstates, ss_state* xx_state);

void convert(struct py_dfa* d, unsigned xx_nstates, ss_state* xx_state);

static void makedfa(gr, nf, d)struct py_nfa_grammar* gr;
							  struct py_nfa* nf;
							  struct py_dfa* d;
{
	int nbits = nf->count;
	py_bitset_t ss;
	unsigned xx_nstates;
	ss_state* xx_state, * yy;
	ss_arc* zz;
	unsigned istate, jstate, iarc, jarc, ibit;
	struct py_nfa_state* st;
	struct py_nfa_arc* ar;

	ss = py_bitset_new(nbits);
	addclosure(ss, nf, nf->start);
	xx_state = malloc(sizeof(ss_state));
	if(xx_state == NULL) {
		py_fatal("no mem for xx_state in makedfa");
	}
	xx_nstates = 1;
	yy = &xx_state[0];
	yy->ss_ss = ss;
	yy->ss_narcs = 0;
	yy->ss_arc = NULL;
	yy->ss_deleted = 0;
	yy->ss_finish = PY_TESTBIT(ss, nf->finish);
	if(yy->ss_finish) {
		printf(
				"Error: nonterminal '%s' may produce empty.\n", nf->name);
	}

	/* This algorithm is from a book written before
	   the invention of structured programming... */

	/* For each unmarked state... */
	for(istate = 0; istate < xx_nstates; ++istate) {
		yy = &xx_state[istate];
		ss = yy->ss_ss;
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
				for(jarc = 0; jarc < yy->ss_narcs; ++jarc) {
					zz = &yy->ss_arc[jarc];
					if(ar->label == zz->sa_label) {
						goto found;
					}
				}
				/* Add new arc for this state */
				/* TODO: Leaky realloc. */
				yy->ss_arc = realloc(
						yy->ss_arc, (yy->ss_narcs + 1) * sizeof(ss_arc));
				if(yy->ss_arc == NULL) {
					py_fatal("out of mem");
				}
				zz = &yy->ss_arc[yy->ss_narcs++];
				zz->sa_label = ar->label;
				zz->sa_bitset = py_bitset_new(nbits);
				zz->sa_arrow = -1;
				found:;
				/* Add destination */
				addclosure(zz->sa_bitset, nf, ar->arrow);
			}
		}
		/* Now look up all the arrow states */
		for(jarc = 0; jarc < xx_state[istate].ss_narcs; jarc++) {
			zz = &xx_state[istate].ss_arc[jarc];
			for(jstate = 0; jstate < xx_nstates; jstate++) {
				if(py_bitset_cmp(
						zz->sa_bitset, xx_state[jstate].ss_ss, nbits)) {
					zz->sa_arrow = jstate;
					goto done;
				}
			}
			xx_state = realloc(xx_state, (xx_nstates + 1) * sizeof(ss_state));
			if(xx_state == NULL) {
				py_fatal("out of mem");
			}
			zz->sa_arrow = xx_nstates;
			yy = &xx_state[xx_nstates++];
			yy->ss_ss = zz->sa_bitset;
			yy->ss_narcs = 0;
			yy->ss_arc = NULL;
			yy->ss_deleted = 0;
			yy->ss_finish = PY_TESTBIT(yy->ss_ss, nf->finish);
			done:;
		}
	}

	if(debugging) {
		printssdfa(
				xx_nstates, xx_state, nbits, &gr->labellist, "before minimizing");
	}

	simplify(xx_nstates, xx_state);

	if(debugging) {
		printssdfa(
				xx_nstates, xx_state, nbits, &gr->labellist, "after minimizing");
	}

	convert(d, xx_nstates, xx_state);

	/* XXX cleanup */
}

void printssdfa(
		unsigned xx_nstates, ss_state* xx_state, unsigned nbits,
		struct py_labellist* ll, char* msg) {

	unsigned i, ibit, iarc;
	ss_state* yy;
	ss_arc* zz;

	printf("Subset DFA %s\n", msg);
	for(i = 0; i < xx_nstates; i++) {
		yy = &xx_state[i];
		if(yy->ss_deleted) {
			continue;
		}
		printf(" Subset %d", i);
		if(yy->ss_finish) {
			printf(" (finish)");
		}
		printf(" { ");
		for(ibit = 0; ibit < nbits; ibit++) {
			if(PY_TESTBIT(yy->ss_ss, ibit)) {
				printf("%d ", ibit);
			}
		}
		printf("}\n");
		for(iarc = 0; iarc < yy->ss_narcs; iarc++) {
			zz = &yy->ss_arc[iarc];
			printf(
					"  Arc to state %d, label %s\n", zz->sa_arrow,
					py_label_repr(&ll->label[zz->sa_label]));
		}
	}
}


/* PART THREE -- SIMPLIFY DFA */

/* Simplify the DFA by repeatedly eliminating states that are
   equivalent to another oner. This is NOT Algorithm 3.3 from
   [Aho&Ullman 77]. It does not always finds the minimal DFA,
   but it does usually make a much smaller one...  (For an example
   of sub-optimal behaviour, try S: x a b+ | y a b+.)
*/

static int samestate(s1, s2)ss_state* s1, * s2;
{
	unsigned i;

	if(s1->ss_narcs != s2->ss_narcs || s1->ss_finish != s2->ss_finish) {
		return 0;
	}
	for(i = 0; i < s1->ss_narcs; i++) {
		if(s1->ss_arc[i].sa_arrow != s2->ss_arc[i].sa_arrow ||
		   s1->ss_arc[i].sa_label != s2->ss_arc[i].sa_label) {
			return 0;
		}
	}
	return 1;
}

static void renamestates(xx_nstates, xx_state, from, to)unsigned xx_nstates;
														ss_state* xx_state;
														unsigned from, to;
{
	unsigned i, j;

	if(debugging) {
		printf("Rename state %d to %d.\n", from, to);
	}
	for(i = 0; i < xx_nstates; i++) {
		if(xx_state[i].ss_deleted) {
			continue;
		}
		for(j = 0; j < xx_state[i].ss_narcs; j++) {
			if(xx_state[i].ss_arc[j].sa_arrow == from) {
				xx_state[i].ss_arc[j].sa_arrow = to;
			}
		}
	}
}

void simplify(int xx_nstates, ss_state* xx_state) {
	int changes;
	int i, j;

	do {
		changes = 0;
		for(i = 1; i < xx_nstates; i++) {
			if(xx_state[i].ss_deleted) {
				continue;
			}
			for(j = 0; j < i; j++) {
				if(xx_state[j].ss_deleted) {
					continue;
				}
				if(samestate(&xx_state[i], &xx_state[j])) {
					xx_state[i].ss_deleted++;
					renamestates(xx_nstates, xx_state, i, j);
					changes++;
					break;
				}
			}
		}
	} while(changes);
}


/* PART FOUR -- GENERATE PARSING TABLES */

/* Convert the DFA into a grammar that can be used by our parser */

void convert(struct py_dfa* d, unsigned xx_nstates, ss_state* xx_state) {
	unsigned i, j;
	ss_state* yy;
	ss_arc* zz;

	for(i = 0; i < xx_nstates; i++) {
		yy = &xx_state[i];
		if(yy->ss_deleted) {
			continue;
		}
		yy->ss_rename = py_dfa_add_state(d);
	}

	for(i = 0; i < xx_nstates; i++) {
		yy = &xx_state[i];
		if(yy->ss_deleted) {
			continue;
		}
		for(j = 0; j < yy->ss_narcs; j++) {
			zz = &yy->ss_arc[j];
			py_dfa_add_arc(
					d, yy->ss_rename, xx_state[zz->sa_arrow].ss_rename,
					zz->sa_label);
		}
		if(yy->ss_finish) {
			py_dfa_add_arc(d, yy->ss_rename, yy->ss_rename, 0);
		}
	}

	d->initial = 0;
}


/* PART FIVE -- GLUE IT ALL TOGETHER */

static struct py_grammar* maketables(gr)struct py_nfa_grammar* gr;
{
	unsigned i;
	struct py_nfa* nf;
	struct py_dfa* d;
	struct py_grammar* g;

	if(gr->count == 0) {
		return NULL;
	}
	g = py_grammar_new(gr->nfas[0]->type);
	/* XXX first rule must be start rule */
	g->labels = gr->labellist;

	for(i = 0; i < gr->count; i++) {
		nf = gr->nfas[i];
		if(debugging) {
			printf("Dump of NFA for '%s' ...\n", nf->name);
			dumpnfa(&gr->labellist, nf);
		}
		fprintf(stderr, "Making DFA for '%s' ...\n", nf->name);
		d = py_grammar_add_dfa(g, nf->type, nf->name);
		makedfa(gr, gr->nfas[i], d);
	}

	return g;
}

struct py_grammar* pgen(n)struct py_node* n;
{
	struct py_nfa_grammar* gr;
	struct py_grammar* g;

	gr = py_node_compile_meta(n);
	g = maketables(gr);
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
