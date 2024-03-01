/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Grammar subroutines needed by parser */

#include <assert.h>

#include <python/grammar.h>
#include <python/token.h>

/* Return the DFA for the given type */

struct py_dfa* py_grammar_find_dfa(g, type)struct py_grammar* g;
					 int type;
{
	int i;
	struct py_dfa* d;

	for(i = g->count, d = g->dfas; --i >= 0; d++) {
		if(d->type == type) {
			return d;
		}
	}
	assert(0);
	/* NOTREACHED */
}

char* py_label_repr(lb)struct py_label* lb;
{
	static char buf[100];

	if(lb->type == PY_ENDMARKER) {
		return "EMPTY";
	}
	else if(lb->type >= PY_NONTERMINAL) {
		if(lb->str == NULL) {
			sprintf(buf, "NT%d", lb->type);
			return buf;
		}
		else {
			return lb->str;
		}
	}
	else {
		if(lb->str == NULL) {
			return py_token_names[lb->type];
		}
		else {
			sprintf(
					buf, "%.32s(%.32s)", py_token_names[lb->type], lb->str);
			return buf;
		}
	}
}
