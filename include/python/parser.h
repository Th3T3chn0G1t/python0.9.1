/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_PARSER_H
#define PY_PARSER_H

/* Parser interface */

#define PY_MAX_STACK (100)

struct py_stackentry {
	int state; /* State in current DFA */
	struct py_dfa* dfa; /* Current DFA */
	struct py_node* parent; /* Where to add next struct py_node */
};

struct py_stack {
	struct py_stackentry* top; /* Top entry */
	struct py_stackentry base[PY_MAX_STACK];/* Array of stack entries */
	/* NB The stack grows down */
};

struct py_parser {
	struct py_stack stack; /* Stack of parser states */
	struct py_grammar* grammar; /* Grammar to use */
	struct py_node* tree; /* Top of parse tree */
};

struct py_parser* py_parser_new(struct py_grammar*, int);
void py_parser_delete(struct py_parser*);
int py_parser_add(struct py_parser*, int, char*, int);

#endif
