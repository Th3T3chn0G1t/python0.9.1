/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Grammar implementation */

#include <python/token.h>
#include <python/grammar.h>
#include <python/errors.h>

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

	void* newptr = realloc(g->dfas, (g->count + 1) * sizeof(struct py_dfa));
	if(newptr == NULL) {
		free(g->dfas);
		/* TODO: Better EH. */
		py_fatal("no mem to resize dfa in py_grammar_add_dfa");
	}
	g->dfas = newptr;

	d = &g->dfas[g->count++];
	d->type = type;
	d->name = name;
	d->count = 0;
	d->states = NULL;
	d->initial = -1;
	d->first = NULL;

	return d; /* Only use while fresh! */
}

unsigned py_dfa_add_state(struct py_dfa* d) {
	struct py_state* s;
	void* newptr;

	newptr = realloc(d->states, (d->count + 1) * sizeof(struct py_state));
	if(!newptr) {
		free(d->states);
		py_fatal("no mem to resize state in py_dfa_add_state");
	}
	d->states = newptr;

	s = &d->states[d->count++];
	s->count = 0;
	s->arcs = NULL;
	s->accel = NULL;

	return (unsigned) (s - d->states);
}

void py_dfa_add_arc(
		struct py_dfa* d, unsigned from, unsigned to, unsigned lbl) {

	struct py_state* s;
	struct py_arc* a;
	void* newptr;

	/* TODO: Better EH. */
	assert(from < d->count);
	assert(to < d->count);

	s = &d->states[from];

	newptr = realloc(s->arcs, (s->count + 1) * sizeof(struct py_arc));
	if(!newptr) {
		py_fatal("no mem to resize arc list in py_dfa_add_arc");
	}
	s->arcs = newptr;

	a = &s->arcs[s->count++];
	a->label = (unsigned short) lbl;
	a->arrow = (unsigned short) to;
}

unsigned py_labellist_add(struct py_labellist* ll, unsigned type, char* str) {
	unsigned i;
	struct py_label* lb;
	void* newptr;

	for(i = 0; i < ll->count; i++) {
		if(ll->label[i].type == type && strcmp(ll->label[i].str, str) == 0) {
			return i;
		}
	}

	newptr = realloc(ll->label, (ll->count + 1) * sizeof(struct py_label));
	if(newptr == NULL) {
		free(ll->label);
		/* TODO: Better EH. */
		py_fatal("no mem to resize struct py_labellist in py_labellist_add");
	}
	ll->label = newptr;

	lb = &ll->label[ll->count++];
	lb->type = type;
	lb->str = str; /* TODO: strdup(str) ??? */

	return (unsigned) (lb - ll->label);
}

/* Same, but rather dies than adds */

unsigned py_labellist_find(struct py_labellist* ll, unsigned type, char* str) {
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

	if(lb->type == PY_NAME) {
		for(i = 0; i < g->count; i++) {
			if(strcmp(lb->str, g->dfas[i].name) == 0) {
				lb->type = g->dfas[i].type;
				lb->str = NULL;
				return;
			}
		}
		for(i = 0; i < (int) PY_N_TOKENS; i++) {
			if(strcmp(lb->str, py_token_names[i]) == 0) {
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
	else printf("Can't translate label\n");
}

void py_grammar_translate(struct py_grammar* g) {
	unsigned i;

	/* Don't translate EMPTY */
	for(i = PY_LABEL_EMPTY + 1; i < g->labels.count; i++) {
		py_grammar_translate_label(g, &g->labels.label[i]);
	}
}
