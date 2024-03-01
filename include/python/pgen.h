/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser generator interface */

#ifndef PY_PGEN_H
#define PY_PGEN_H

#include <python/grammar.h>
#include <python/node.h>

/* TODO: Python global state. */
extern struct py_grammar py_grammar;
extern struct py_grammar py_meta_grammar;

struct py_grammar* pgen(struct py_node*);

#endif
