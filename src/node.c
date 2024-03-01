/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parse tree node implementation */

#include <stdlib.h>

#include <python/node.h>

struct py_node* py_tree_new(type)int type;
{
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

#define XXX 3 /* Node alignment factor to speed up realloc */
#define XXXROUNDUP(n) ((n) == 1 ? 1 : ((n) + XXX - 1) / XXX * XXX)

struct py_node* py_tree_add(n1, type, str, lineno)struct py_node* n1;
									 int type;
									 char* str;
									 int lineno;
{
	int nch = n1->count;
	int nch1 = nch + 1;
	struct py_node* n;
	if(XXXROUNDUP(nch) < nch1) {
		n = n1->children;
		nch1 = XXXROUNDUP(nch1);
		/* TODO: Leaky realloc. */
		n = realloc(n, nch1 * sizeof(struct py_node));
		if(n == NULL) {
			return NULL;
		}
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

/* Forward */
static void freechildren(struct py_node*);


void py_tree_delete(n)struct py_node* n;
{
	if(n != NULL) {
		freechildren(n);
		free(n);
	}
}

static void freechildren(n)struct py_node* n;
{
	int i;
	for(i = n->count; --i >= 0;) {
		freechildren(&n->children[i]);
	}
	if(n->children != NULL) {
		free(n->children);
	}
	if(n->str != NULL) {
		free(n->str);
	}
}
