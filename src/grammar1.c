/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/*
 * TODO: This is runtime (not pgen-only) grammar stuff -- can definitely be
 * 		 Given a better name.
 */

/* Grammar subroutines needed by parser */

#include <python/grammar.h>
#include <python/token.h>

/* Return the DFA for the given type */

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

char* py_label_repr(struct py_label* lb) {
	/* This looks questionable but should be fine. */
	static char buf[100];

	if(lb->type == PY_ENDMARKER) return "EMPTY";

	else if(lb->type >= PY_NONTERMINAL) {
		if(lb->str == NULL) {
			sprintf(buf, "NT%d", lb->type);
			return buf;
		}
		else return lb->str;
	}
	else {
		if(lb->str == NULL) return py_token_names[lb->type];
		else {
			sprintf(buf, "%.32s(%.32s)", py_token_names[lb->type], lb->str);
			return buf;
		}
	}
}
