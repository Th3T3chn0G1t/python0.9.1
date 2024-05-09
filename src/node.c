/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parse tree node implementation */

#include <python/std.h>
#include <python/node.h>

struct py_node* py_tree_new(int type) {
	struct py_node* n = malloc(sizeof(struct py_node));
	if(n == NULL) {
		return NULL;
	}
	n->type = type;
	n->str = NULL;
	n->lineno = 0;
	n->count = 0;
	n->children = NULL;
	return n;
}

/* Node alignment factor to speed up realloc */
#define PY_SCALE_FACTOR (3)
#define PY_ROUND_UP(n) \
	((n) == 1 ? 1 : \
		((n) + PY_SCALE_FACTOR - 1) / PY_SCALE_FACTOR * PY_SCALE_FACTOR)

struct py_node* py_tree_add(
		struct py_node* n1, int type, char* str, unsigned lineno) {

	unsigned nch = n1->count;
	unsigned nch1 = nch + 1;
	struct py_node* n;

	if(PY_ROUND_UP(nch) < nch1) {
		void* newptr;

		n = n1->children;
		nch1 = PY_ROUND_UP(nch1);

		newptr = realloc(n, nch1 * sizeof(struct py_node));
		if(newptr == NULL) {
			free(n);
			return NULL;
		}
		n = newptr;

		n1->children = n;
	}
	n = &n1->children[n1->count++];
	n->type = type;
	n->str = str;
	n->lineno = lineno;
	n->count = 0;
	n->children = NULL;
	return n;
}

static void py_tree_free_children(struct py_node* n) {
	unsigned i;

	for(i = 0; i < n->count; ++i) py_tree_free_children(&n->children[i]);

	if(n->children != NULL) free(n->children);
	if(n->str != NULL) free(n->str);
}

void py_tree_delete(struct py_node* n) {
	if(n != NULL) {
		py_tree_free_children(n);
		free(n);
	}
}
