/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Tokenizer interface */

#ifndef PY_TOKENIZER_H
#define PY_TOKENIZER_H

#include <python/std.h>
#include <python/result.h>

/* Max indentation level */
#define PY_MAX_INDENT (100)

/* Tokenizer state */
struct py_tokenizer {
	/* Input state; buf <= cur <= inp <= end */
	/* NB an entire token must fit in the buffer */
	char* buf; /* Input buffer */
	char* cur; /* Next character in buffer */
	char* inp; /* End of data in buffer */
	char* end; /* End of input buffer */
	enum py_result done;
	FILE* fp; /* Rest of input; NULL if tokenizing a string */
	int indent; /* Current indentation index */
	int indstack[PY_MAX_INDENT]; /* Stack of indents */
	int atbol; /* Nonzero if at begin of new line */
	int pendin; /* Pending indents (if > 0) or dedents (if < 0) */
	char* prompt; /* For interactive prompting */
	char* nextprompt;
	unsigned lineno; /* Current line number */
};

void py_tokenizer_delete(struct py_tokenizer*);
struct py_tokenizer* py_tokenizer_setup_file(FILE*, char* ps1, char* ps2);
unsigned py_tokenizer_get(struct py_tokenizer*, char**, char**);

#endif
