/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* List a node on a file */

#include <python/token.h>
#include <python/node.h>

/* Forward */
static void list1node(FILE*, struct py_node*);

static int level, atbol;

void py_tree_list(fp, n)FILE* fp;
						struct py_node* n;
{
	level = 0;
	atbol = 1;
	list1node(fp, n);
}

static void list1node(fp, n)FILE* fp;
							struct py_node* n;
{
	if(n == 0) {
		return;
	}
	if(n->type >= PY_NONTERMINAL) {
		unsigned i;
		for(i = 0; i < n->count; i++) {
			list1node(fp, &n->children[i]);
		}
	}
	else {
		switch(n->type) {
			case PY_INDENT:++level;
				break;
			case PY_DEDENT:--level;
				break;
			default:
				if(atbol) {
					int i;
					for(i = 0; i < level; ++i) {
						fprintf(fp, "\t");
					}
					atbol = 0;
				}
				if(n->type == PY_NEWLINE) {
					if(n->str != NULL) {
						fprintf(fp, "%s", n->str);
					}
					fprintf(fp, "\n");
					atbol = 1;
				}
				else {
					fprintf(fp, "%s ", n->str);
				}
				break;
		}
	}
}
