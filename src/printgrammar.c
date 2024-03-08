/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Print a bunch of C initializers that represent a grammar */

#include <python/grammar.h>

/* Forward */
static void py_grammar_print_states(struct py_grammar*, FILE*);

static void py_grammar_print_dfas(struct py_grammar*, FILE*);

static void py_grammar_print_labels(struct py_grammar*, FILE*);

/* TODO: Stream EH. */

void py_grammar_print(struct py_grammar* g, FILE* fp) {
	fprintf(fp, "#include <python/grammar.h>\n");
	py_grammar_print_dfas(g, fp);
	py_grammar_print_labels(g, fp);
	fprintf(fp, "struct py_grammar py_grammar = {\n");
	fprintf(fp, "\t%d,\n", g->count);
	fprintf(fp, "\tdfas,\n");
	fprintf(fp, "\t{%d, labels},\n", g->labels.count);
	fprintf(fp, "\t%d,\n", g->start);
	fprintf(fp, "\t0\n");
	fprintf(fp, "};\n");
}

void py_grammar_print_nonterminals(struct py_grammar* g, FILE* fp) {
	struct py_dfa* d;
	unsigned i;

	d = g->dfas;
	for(i = 0; i < g->count; i++, d++) {
		fprintf(fp, "#define %s (%d)\n", d->name, d->type);
	}
}

static void py_dfa_print_arcs(unsigned i, struct py_dfa* d, FILE* fp) {
	struct py_arc* a;
	struct py_state* s;
	unsigned j, k;

	s = d->states;
	for(j = 0; j < d->count; j++, s++) {
		fprintf(
				fp, "static struct py_arc arcs_%d_%d[%d] = {\n", i, j,
				s->count);
		a = s->arcs;
		for(k = 0; k < s->count; k++, a++) {
			fprintf(fp, "\t{%d, %d},\n", a->label, a->arrow);
		}
		fprintf(fp, "};\n");
	}
}

static void py_grammar_print_states(struct py_grammar* g, FILE* fp) {
	struct py_state* s;
	struct py_dfa* d;
	unsigned i, j;

	d = g->dfas;
	for(i = 0; i < g->count; i++, d++) {
		py_dfa_print_arcs(i, d, fp);
		fprintf(fp, "static struct py_state states_%d[%d] = {\n", i, d->count);
		s = d->states;
		for(j = 0; j < d->count; j++, s++) {
			fprintf(fp, "\t{%d, arcs_%d_%d, 0, 0, 0, 0},\n", s->count, i, j);
		}
		fprintf(fp, "};\n");
	}
}

static void py_grammar_print_dfas(struct py_grammar* g, FILE* fp) {
	struct py_dfa* d;
	unsigned i, j;

	py_grammar_print_states(g, fp);
	fprintf(fp, "static struct py_dfa dfas[%d] = {\n", g->count);
	d = g->dfas;

	for(i = 0; i < g->count; i++, d++) {
		fprintf(
				fp, "\t{%d, \"%s\", %d, %d, states_%d,\n", d->type, d->name,
				d->initial, d->count, i);
		fprintf(fp, "\t (py_byte_t*) \"");

		for(j = 0; j < PY_NBYTES(g->labels.count); j++) {
			fprintf(fp, "\\%03o", d->first[j] & 0xff);
		}

		fprintf(fp, "\"},\n");
	}

	fprintf(fp, "};\n");
}

static void py_grammar_print_labels(struct py_grammar* g, FILE* fp) {
	struct py_label* l;
	unsigned i;

	fprintf(fp, "static struct py_label labels[%d] = {\n", g->labels.count);
	l = g->labels.label;

	for(i = 0; i < g->labels.count; i++, l++) {
		if(l->str == NULL) fprintf(fp, "\t{%d, 0},\n", l->type);
		else fprintf(fp, "\t{%d, \"%s\"},\n", l->type, l->str);
	}

	fprintf(fp, "};\n");
}
