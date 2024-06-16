/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Grammar interface */

#ifndef PY_GRAMMAR_H
#define PY_GRAMMAR_H

#include <python/std.h>
#include <python/bitset.h>

/* A label of an arc */
struct py_label {
	int type;
	char* str;
};

/* Label number 0 is by definition the empty label */
#define PY_LABEL_EMPTY (0)

/* A list of labels */
struct py_labellist {
	unsigned count;
	struct py_label* label;
};

/* An arc from one state to another */
/* TODO: Does these being shorts have any purpose? */
struct py_arc {
	unsigned short label; /* Label of this arc */
	unsigned short arrow; /* State where this arc goes to */
};

/* A state in a DFA */
struct py_state {
	unsigned count;
	struct py_arc* arcs; /* Array of arcs */

	/* Optional accelerators */
	int lower; /* Lowest label index */
	int upper; /* Highest label index */
	int* accel; /* Accelerator */
	int accept; /* Nonzero for accepting state */
};

/* A DFA */
struct py_dfa {
	int type; /* Non-terminal this represents */
	char* name; /* For printing */
	int initial; /* Initial state */

	unsigned count;
	struct py_state* states; /* Array of states */
	py_bitset_t first;
};

/* A grammar */
struct py_grammar {
	unsigned count;
	struct py_dfa* dfas; /* Array of DFAs */

	struct py_labellist labels;
	int start; /* Start symbol of the grammar */
	int accel; /* Set if accelerators present */
};

/* FUNCTIONS */
struct py_grammar* py_grammar_new(int);
struct py_dfa* py_grammar_add_dfa(struct py_grammar*, int, char*);
struct py_dfa* py_grammar_find_dfa(struct py_grammar*, int);

void py_grammar_translate(struct py_grammar*);
void py_grammar_add_firsts(struct py_grammar*);
void py_grammar_add_accels(struct py_grammar*);
void py_grammar_delete_accels(void);
void py_grammar_print(struct py_grammar*, FILE*);
void py_grammar_print_nonterminals(struct py_grammar*, FILE*);

int py_dfa_add_state(struct py_dfa*);
void py_dfa_add_arc(struct py_dfa*, unsigned, unsigned, unsigned);

int py_labellist_add(struct py_labellist*, int, char*);
unsigned py_labellist_find(struct py_labellist*, int, char*);

#endif
