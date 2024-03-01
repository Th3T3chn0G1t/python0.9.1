/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser-tokenizer link interface */

#ifndef PY_PARSETOK_H
#define PY_PARSETOK_H

#include <python/std.h>
#include <python/grammar.h>
#include <python/node.h>

int py_parse_file(FILE*, char*, struct py_grammar*, int, char*, char*, struct py_node**);

#endif
