/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser generator */

/* XXX This file is not yet fully PROTOized */

/* For a description, see the comments at end of this file */

#include <python/token.h>
#include <python/errors.h>
#include <python/node.h>
#include <python/grammar.h>
#include <python/metagrammar.h>
#include <python/pgen.h>

extern int debugging;


/* PART ONE -- CONSTRUCT NFA -- Cf. Algorithm 3.2 from [Aho&Ullman 77] */

typedef struct _nfaarc {
	int ar_label;
	int ar_arrow;
} nfaarc;

typedef struct _nfastate {
	int st_narcs;
	nfaarc* st_arc;
} nfastate;

typedef struct _nfa {
	int nf_type;
	char* nf_name;
	int nf_nstates;
	nfastate* nf_state;
	int nf_start, nf_finish;
} nfa;

static int addnfastate(nf)nfa* nf;
{
	nfastate* st;

	/* TODO: Leaky realloc. */
	nf->nf_state = realloc(
			nf->nf_state, (nf->nf_nstates + 1) * sizeof(nfastate));
	if(nf->nf_state == NULL) {
		py_fatal("out of mem");
	}
	st = &nf->nf_state[nf->nf_nstates++];
	st->st_narcs = 0;
	st->st_arc = NULL;
	return st - nf->nf_state;
}

static void addnfaarc(nf, from, to, lbl)nfa* nf;
										int from, to, lbl;
{
	nfastate* st;
	nfaarc* ar;

	st = &nf->nf_state[from];
	st->st_arc = realloc(st->st_arc, (st->st_narcs + 1) * sizeof(nfaarc));
	if(st->st_arc == NULL) {
		py_fatal("out of mem");
	}
	ar = &st->st_arc[st->st_narcs++];
	ar->ar_label = lbl;
	ar->ar_arrow = to;
}

static nfa* newnfa(name)char* name;
{
	nfa* nf;
	static int type = PY_NONTERMINAL; /* All types will be disjunct */

	nf = malloc(sizeof(nfa));
	if(nf == NULL) {
		py_fatal("no mem for new nfa");
	}
	nf->nf_type = type++;
	nf->nf_name = name; /* XXX strdup(name) ??? */
	nf->nf_nstates = 0;
	nf->nf_state = NULL;
	nf->nf_start = nf->nf_finish = -1;
	return nf;
}

typedef struct _nfagrammar {
	int gr_nnfas;
	nfa** gr_nfa;
	struct py_labellist gr_ll;
} nfagrammar;

static nfagrammar* newnfagrammar() {
	nfagrammar* gr;

	gr = malloc(sizeof(nfagrammar));
	if(gr == NULL) {
		py_fatal("no mem for new nfa grammar");
	}
	gr->gr_nnfas = 0;
	gr->gr_nfa = NULL;
	gr->gr_ll.count = 0;
	gr->gr_ll.label = NULL;
	py_labellist_add(&gr->gr_ll, PY_ENDMARKER, "EMPTY");
	return gr;
}

static nfa* addnfa(gr, name)nfagrammar* gr;
							char* name;
{
	nfa* nf;

	nf = newnfa(name);
	gr->gr_nfa = realloc(gr->gr_nfa, (gr->gr_nnfas + 1) * sizeof(nfa*));
	if(gr->gr_nfa == NULL) {
		py_fatal("out of mem");
	}
	gr->gr_nfa[gr->gr_nnfas++] = nf;
	py_labellist_add(&gr->gr_ll, PY_NAME, nf->nf_name);
	return nf;
}

#ifdef _DEBUG

static char REQNFMT[] = "metacompile: less than %d children\n";

#define REQN(i, count) \
       if (i < count) { \
               fprintf(stderr, REQNFMT, count); \
               abort(); \
       }

#else
#define REQN(i, count) /* empty */
#endif

void compile_rule(nfagrammar* gr, struct py_node* n);

static nfagrammar* metacompile(n)struct py_node* n;
{
	nfagrammar* gr;
	int i;

	fprintf(stderr, "Compiling (meta-) parse tree into NFA grammar\n");
	gr = newnfagrammar();
	PY_REQ(n, PY_MSTART);
	i = n->count - 1; /* Last child is PY_ENDMARKER */
	n = n->children;
	for(; --i >= 0; n++) {
		if(n->type != PY_NEWLINE) {
			compile_rule(gr, n);
		}
	}
	return gr;
}

void compile_rhs(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb);

void compile_rule(nfagrammar* gr, struct py_node* n) {
	nfa* nf;

	PY_REQ(n, PY_RULE);
	REQN(n->count, 4);
	n = n->children;
	PY_REQ(n, PY_NAME);
	nf = addnfa(gr, n->str);
	n++;
	PY_REQ(n, PY_COLON);
	n++;
	PY_REQ(n, PY_RHS);
	compile_rhs(&gr->gr_ll, nf, n, &nf->nf_start, &nf->nf_finish);
	n++;
	PY_REQ(n, PY_NEWLINE);
}

void compile_alt(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb);

void compile_rhs(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb) {
	int i;
	int a, b;

	PY_REQ(n, PY_RHS);
	i = n->count;
	REQN(i, 1);
	n = n->children;
	PY_REQ(n, PY_ALT);
	compile_alt(ll, nf, n, pa, pb);
	if(--i <= 0) {
		return;
	}
	n++;
	a = *pa;
	b = *pb;
	*pa = addnfastate(nf);
	*pb = addnfastate(nf);
	addnfaarc(nf, *pa, a, PY_LABEL_EMPTY);
	addnfaarc(nf, b, *pb, PY_LABEL_EMPTY);
	for(; --i >= 0; n++) {
		PY_REQ(n, PY_VBAR);
		REQN(i, 1);
		--i;
		n++;
		PY_REQ(n, PY_ALT);
		compile_alt(ll, nf, n, &a, &b);
		addnfaarc(nf, *pa, a, PY_LABEL_EMPTY);
		addnfaarc(nf, b, *pb, PY_LABEL_EMPTY);
	}
}

void compile_item(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb);

void compile_alt(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb) {
	int i;
	int a, b;

	PY_REQ(n, PY_ALT);
	i = n->count;
	REQN(i, 1);
	n = n->children;
	PY_REQ(n, PY_ITEM);
	compile_item(ll, nf, n, pa, pb);
	--i;
	n++;
	for(; --i >= 0; n++) {
		if(n->type == PY_COMMA) { /* XXX Temporary */
			REQN(i, 1);
			--i;
			n++;
		}
		PY_REQ(n, PY_ITEM);
		compile_item(ll, nf, n, &a, &b);
		addnfaarc(nf, *pb, a, PY_LABEL_EMPTY);
		*pb = b;
	}
}

void compile_atom(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb);

void compile_item(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb) {
	int i;
	int a, b;

	PY_REQ(n, PY_ITEM);
	i = n->count;
	REQN(i, 1);
	n = n->children;
	if(n->type == PY_LSQB) {
		REQN(i, 3);
		n++;
		PY_REQ(n, PY_RHS);
		*pa = addnfastate(nf);
		*pb = addnfastate(nf);
		addnfaarc(nf, *pa, *pb, PY_LABEL_EMPTY);
		compile_rhs(ll, nf, n, &a, &b);
		addnfaarc(nf, *pa, a, PY_LABEL_EMPTY);
		addnfaarc(nf, b, *pb, PY_LABEL_EMPTY);
		REQN(i, 1);
		n++;
		PY_REQ(n, PY_RSQB);
	}
	else {
		compile_atom(ll, nf, n, pa, pb);
		if(--i <= 0) {
			return;
		}
		n++;
		addnfaarc(nf, *pb, *pa, PY_LABEL_EMPTY);
		if(n->type == PY_STAR) {
			*pb = *pa;
		}
		else
			PY_REQ(n, PY_PLUS);
	}
}

void compile_atom(
		struct py_labellist* ll, nfa* nf, struct py_node* n, int* pa, int* pb) {
	int i;

	PY_REQ(n, PY_ATOM);
	i = n->count;
	REQN(i, 1);
	n = n->children;
	if(n->type == PY_LPAR) {
		REQN(i, 3);
		n++;
		PY_REQ(n, PY_RHS);
		compile_rhs(ll, nf, n, pa, pb);
		n++;
		PY_REQ(n, PY_RPAR);
	}
	else if(n->type == PY_NAME || n->type == PY_STRING) {
		*pa = addnfastate(nf);
		*pb = addnfastate(nf);
		addnfaarc(nf, *pa, *pb, py_labellist_add(ll, n->type, n->str));
	}
	else
		PY_REQ(n, PY_NAME);
}

static void dumpstate(ll, nf, istate)struct py_labellist* ll;
									 nfa* nf;
									 int istate;
{
	nfastate* st;
	int i;
	nfaarc* ar;

	printf(
			"%c%2d%c", istate == nf->nf_start ? '*' : ' ', istate,
			istate == nf->nf_finish ? '.' : ' ');
	st = &nf->nf_state[istate];
	ar = st->st_arc;
	for(i = 0; i < st->st_narcs; i++) {
		if(i > 0) {
			printf("\n    ");
		}
		printf(
				"-> %2d  %s", ar->ar_arrow,
				py_label_repr(&ll->label[ar->ar_label]));
		ar++;
	}
	printf("\n");
}

static void dumpnfa(ll, nf)struct py_labellist* ll;
						   nfa* nf;
{
	int i;

	printf(
			"NFA '%s' has %d states; start %d, finish %d\n", nf->nf_name,
			nf->nf_nstates, nf->nf_start, nf->nf_finish);
	for(i = 0; i < nf->nf_nstates; i++) {
		dumpstate(ll, nf, i);
	}
}


/* PART TWO -- CONSTRUCT DFA -- Algorithm 3.1 from [Aho&Ullman 77] */

static void addclosure(ss, nf, istate)py_bitset_t ss;
									  nfa* nf;
									  int istate;
{
	if(py_bitset_add(ss, istate)) {
		nfastate* st = &nf->nf_state[istate];
		nfaarc* ar = st->st_arc;
		int i;

		for(i = st->st_narcs; --i >= 0;) {
			if(ar->ar_label == PY_LABEL_EMPTY) {
				addclosure(ss, nf, ar->ar_arrow);
			}
			ar++;
		}
	}
}

typedef struct _ss_arc {
	py_bitset_t sa_bitset;
	int sa_arrow;
	int sa_label;
} ss_arc;

typedef struct _ss_state {
	py_bitset_t ss_ss;
	int ss_narcs;
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
		int xx_nstates, ss_state* xx_state, int nbits, struct py_labellist* ll,
		char* msg);

void simplify(int xx_nstates, ss_state* xx_state);

void convert(struct py_dfa* d, int xx_nstates, ss_state* xx_state);

static void makedfa(gr, nf, d)nfagrammar* gr;
							  nfa* nf;
							  struct py_dfa* d;
{
	int nbits = nf->nf_nstates;
	py_bitset_t ss;
	int xx_nstates;
	ss_state* xx_state, * yy;
	ss_arc* zz;
	int istate, jstate, iarc, jarc, ibit;
	nfastate* st;
	nfaarc* ar;

	ss = py_bitset_new(nbits);
	addclosure(ss, nf, nf->nf_start);
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
	yy->ss_finish = PY_TESTBIT(ss, nf->nf_finish);
	if(yy->ss_finish) {
		printf(
				"Error: nonterminal '%s' may produce empty.\n", nf->nf_name);
	}

	/* This algorithm is from a book written before
	   the invention of structured programming... */

	/* For each unmarked state... */
	for(istate = 0; istate < xx_nstates; ++istate) {
		yy = &xx_state[istate];
		ss = yy->ss_ss;
		/* For all its states... */
		for(ibit = 0; ibit < nf->nf_nstates; ++ibit) {
			if(!PY_TESTBIT(ss, ibit)) {
				continue;
			}
			st = &nf->nf_state[ibit];
			/* For all non-empty arcs from this state... */
			for(iarc = 0; iarc < st->st_narcs; iarc++) {
				ar = &st->st_arc[iarc];
				if(ar->ar_label == PY_LABEL_EMPTY) {
					continue;
				}
				/* Look up in list of arcs from this state */
				for(jarc = 0; jarc < yy->ss_narcs; ++jarc) {
					zz = &yy->ss_arc[jarc];
					if(ar->ar_label == zz->sa_label) {
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
				zz->sa_label = ar->ar_label;
				zz->sa_bitset = py_bitset_new(nbits);
				zz->sa_arrow = -1;
				found:;
				/* Add destination */
				addclosure(zz->sa_bitset, nf, ar->ar_arrow);
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
			yy->ss_finish = PY_TESTBIT(yy->ss_ss, nf->nf_finish);
			done:;
		}
	}

	if(debugging) {
		printssdfa(
				xx_nstates, xx_state, nbits, &gr->gr_ll, "before minimizing");
	}

	simplify(xx_nstates, xx_state);

	if(debugging) {
		printssdfa(
				xx_nstates, xx_state, nbits, &gr->gr_ll, "after minimizing");
	}

	convert(d, xx_nstates, xx_state);

	/* XXX cleanup */
}

void printssdfa(
		int xx_nstates, ss_state* xx_state, int nbits, struct py_labellist* ll,
		char* msg) {

	int i, ibit, iarc;
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
	int i;

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

static void renamestates(xx_nstates, xx_state, from, to)int xx_nstates;
														ss_state* xx_state;
														int from, to;
{
	int i, j;

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

void convert(struct py_dfa* d, int xx_nstates, ss_state* xx_state) {
	int i, j;
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

static struct py_grammar* maketables(gr)nfagrammar* gr;
{
	int i;
	nfa* nf;
	struct py_dfa* d;
	struct py_grammar* g;

	if(gr->gr_nnfas == 0) {
		return NULL;
	}
	g = py_grammar_new(gr->gr_nfa[0]->nf_type);
	/* XXX first rule must be start rule */
	g->labels = gr->gr_ll;

	for(i = 0; i < gr->gr_nnfas; i++) {
		nf = gr->gr_nfa[i];
		if(debugging) {
			printf("Dump of NFA for '%s' ...\n", nf->nf_name);
			dumpnfa(&gr->gr_ll, nf);
		}
		fprintf(stderr, "Making DFA for '%s' ...\n", nf->nf_name);
		d = py_grammar_add_dfa(g, nf->nf_type, nf->nf_name);
		makedfa(gr, gr->gr_nfa[i], d);
	}

	return g;
}

struct py_grammar* pgen(n)struct py_node* n;
{
	nfagrammar* gr;
	struct py_grammar* g;

	gr = metacompile(n);
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
