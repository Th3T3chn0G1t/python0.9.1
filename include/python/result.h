/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Error codes passed around between file input, tokenizer, parser and
 * interpreter. This was necessary, so we can turn them into Python
 * exceptions at a higher level.
 */

#ifndef PY_RESULT_H
#define PY_RESULT_H

/* TODO: Use for more things in native land. */
enum py_result {
	PY_RESULT_OK = 10, /* No error */
	PY_RESULT_EOF = 11, /* (Unexpected) EOF read */
	PY_RESULT_TOKEN = 13, /* Bad token */
	PY_RESULT_SYNTAX = 14, /* Syntax error */
	PY_RESULT_OOM = 15, /* Ran out of memory */
	PY_RESULT_DONE = 16, /* Parsing complete */

	PY_RESULT_ERROR = 20 /* Generic error */
};

#endif
