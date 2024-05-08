/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Grammar implementation */

#include <python/token.h>
#include <python/grammar.h>
#include <python/errors.h>

/* TODO: Python global state. */
extern int debugging;

struct py_grammar* py_grammar_new(int start) {
	struct py_grammar* g;

	g = malloc(sizeof(struct py_grammar));
	/* TODO: Better EH. */
	if(g == NULL) py_fatal("no mem for new grammar");

	g->count = 0;
	g->dfas = NULL;
	g->start = start;
	g->labels.count = 0;
	g->labels.label = NULL;

	return g;
}

struct py_dfa* py_grammar_add_dfa(struct py_grammar* g, int type, char* name) {
	struct py_dfa* d;

	/* TODO: Leaky realloc. */
	g->dfas = realloc(g->dfas, (g->count + 1) * sizeof(struct py_dfa));
	if(g->dfas == NULL) py_fatal("no mem to resize dfa in py_grammar_add_dfa");

	d = &g->dfas[g->count++];
	d->type = type;
	d->name = name;
	d->count = 0;
	d->states = NULL;
	d->initial = -1;
	d->first = NULL;

	return d; /* Only use while fresh! */
}

int py_dfa_add_state(struct py_dfa* d) {
	struct py_state* s;

	d->states = realloc(d->states, (d->count + 1) * sizeof(struct py_state));
	if(d->states == NULL) {
		py_fatal("no mem to resize state in py_dfa_add_state");
	}

	s = &d->states[d->count++];
	s->count = 0;
	s->arcs = NULL;
	s->accel = NULL;

	return s - d->states;
}

void py_dfa_add_arc(
		struct py_dfa* d, unsigned from, unsigned to, unsigned lbl) {

	struct py_state* s;
	struct py_arc* a;

	/* TODO: Better EH. */
	assert(from < d->count);
	assert(to < d->count);

	s = &d->states[from];

	s->arcs = realloc(s->arcs, (s->count + 1) * sizeof(struct py_arc));
	if(s->arcs == NULL) {
		py_fatal("no mem to resize arc list in py_dfa_add_arc");
	}

	a = &s->arcs[s->count++];
	a->label = (unsigned short) lbl;
	a->arrow = (unsigned short) to;
}

int py_labellist_add(struct py_labellist* ll, int type, char* str) {
	unsigned i;
	struct py_label* lb;

	for(i = 0; i < ll->count; i++) {
		if(ll->label[i].type == type && strcmp(ll->label[i].str, str) == 0) {
			return i;
		}
	}

	ll->label = realloc(ll->label, (ll->count + 1) * sizeof(struct py_label));
	if(ll->label == NULL) {
		py_fatal("no mem to resize struct py_labellist in py_labellist_add");
	}

	lb = &ll->label[ll->count++];
	lb->type = type;
	lb->str = str; /* TODO: strdup(str) ??? */

	return lb - ll->label;
}

/* Same, but rather dies than adds */

unsigned py_labellist_find(struct py_labellist* ll, int type, char* str) {
	unsigned i;

	for(i = 0; i < ll->count; i++) {
		if(ll->label[i].type == type /*&&
                       strcmp(ll->label[i].str, str) == 0*/) {
			return i;
		}
	}

	/* TODO: Better EH. */
	fprintf(stderr, "Label %d/'%s' not found\n", type, str);
	abort();
}

static void py_grammar_translate_label(
		struct py_grammar* g, struct py_label* lb) {

	unsigned i;

	if(debugging) {
		printf("Translating label %s ...\n", py_label_repr(lb));
	}

	if(lb->type == PY_NAME) {
		for(i = 0; i < g->count; i++) {
			if(strcmp(lb->str, g->dfas[i].name) == 0) {
				if(debugging) {
					printf(
							"Label %s is non-terminal %d.\n", lb->str,
							g->dfas[i].type);
				}
				lb->type = g->dfas[i].type;
				lb->str = NULL;
				return;
			}
		}
		for(i = 0; i < (int) PY_N_TOKENS; i++) {
			if(strcmp(lb->str, py_token_names[i]) == 0) {
				if(debugging) {
					printf(
							"Label %s is terminal %d.\n", lb->str, i);
				}
				lb->type = i;
				lb->str = NULL;
				return;
			}
		}

		printf("Can't translate PY_NAME label '%s'\n", lb->str);
		return;
	}

	if(lb->type == PY_STRING) {
		if(isalpha(lb->str[1])) {
			char* p;

			if(debugging) {
				printf("Label %s is a keyword\n", lb->str);
			}

			lb->type = PY_NAME;
			lb->str++;

			p = strchr(lb->str, '\'');
			if(p) *p = '\0';
		}
		else {
			if(lb->str[2] == lb->str[0]) {
				int type = (int) py_token_char(lb->str[1]);
				if(type != PY_OP) {
					lb->type = type;
					lb->str = NULL;
				}
				else printf("Unknown PY_OP label %s\n", lb->str);
			}
			else printf("Can't translate PY_STRING label %s\n", lb->str);
		}
	}
	else printf("Can't translate label '%s'\n", py_label_repr(lb));
}

void py_grammar_translate(struct py_grammar* g) {
	unsigned i;

	fprintf(stderr, "Translating labels ...\n");
	/* Don't translate EMPTY */
	for(i = PY_LABEL_EMPTY + 1; i < g->labels.count; i++) {
		py_grammar_translate_label(g, &g->labels.label[i]);
	}
}
