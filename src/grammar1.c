/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* TODO: It feels like this file doesn't need to exist. */

/* Grammar subroutines needed by parser */

#include <python/grammar.h>
#include <python/token.h>

/* Return the DFA for the given type */

struct py_dfa* py_grammar_find_dfa(struct py_grammar* g, int type) {
	int i;
	struct py_dfa* d;

	/* TODO: Signedness. */
	for(i = (int) g->count, d = g->dfas; --i >= 0; d++) {
		if(d->type == type) {
			return d;
		}
	}

	/* TODO: Better EH. */
	abort();
	/* NOTREACHED */
}

char* py_label_repr(struct py_label* lb) {
	/* TODO: Questionable buffer safety. */
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
			sprintf(
					buf, "%.32s(%.32s)", py_token_names[lb->type], lb->str);
			return buf;
		}
	}
}
