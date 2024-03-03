/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parse tree node interface */

#ifndef PY_NODE_H
#define PY_NODE_H

#include <python/std.h>

struct py_node {
	int type;
	char* str;
	int lineno;

	unsigned count;
	struct py_node* children;
};

struct py_node* py_tree_new(int);

void py_tree_delete(struct py_node* n);

struct py_node* py_tree_add(struct py_node*, int, char*, int);

void py_tree_list(FILE*, struct py_node*);

struct py_object* py_tree_eval(
		struct py_node*, const char*, struct py_object*, struct py_object*);

struct py_object* py_tree_run(
		struct py_node*, char*, struct py_object*, struct py_object*);

/* Assert that the type of node is what we expect */
/* TODO: Handle this better. */
#ifndef _DEBUG
# define PY_REQ(n, type) do { /* pass */ } while(0)
#else
# define PY_REQ(n, t) \
    do { \
        if((n)->type != (t)) { \
            fprintf( \
                stderr, "FATAL: node type %d, required %d\n", (n)->type, t); \
            abort(); \
        } \
    } while(0)
#endif

#endif
