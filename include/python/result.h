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

#define PY_RESULT_OK (10) /* No error */
#define PY_RESULT_EOF (11) /* (Unexpected) EOF read */
#define PY_RESULT_INTERRUPT (12) /* Interrupted */
#define PY_RESULT_TOKEN (13) /* Bad token */
#define PY_RESULT_SYNTAX (14) /* Syntax error */
#define PY_RESULT_OOM (15) /* Ran out of memory */
#define PY_RESULT_DONE (16) /* Parsing complete */

#endif
