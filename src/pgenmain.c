/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser generator main program */

/*
 * This expects a filename containing the grammar as argv[1]
 * It writes its output on two files in the current directory:
 * - "graminit.c" gets the grammar as a bunch of initialized data
 * - "graminit.h" gets the grammar's non-terminals as #defines.
 * Error messages and status info during the generation process are
 * written to stdout, or sometimes to stderr.
 */

#include <python/std.h>
#include <python/grammar.h>
#include <python/node.h>
#include <python/parsetok.h>
#include <python/pgen.h>

int debugging;

/* Forward */
struct py_grammar* py_load_grammar(const char*);

int main(int argc, char** argv) {
	struct py_grammar* g;
	FILE* fp;
	char* filename;

	if(argc != 4) {
		fprintf(stderr, "usage: %s grammar outsource outheader\n", argv[0]);
		exit(2);
	}

	filename = argv[1];
	g = py_load_grammar(filename);

	fp = fopen(argv[2], "w");
	if(fp == NULL) {
		perror("graminit.c");
		exit(1);
	}

	py_grammar_print(g, fp);
	fclose(fp);

	fp = fopen(argv[3], "w");
	if(fp == NULL) {
		perror("graminit.h");
		exit(1);
	}

	py_grammar_print_nonterminals(g, fp);
	fclose(fp);

	exit(0);
}

struct py_grammar* py_load_grammar(const char* filename) {
	FILE* fp;
	struct py_node* n;
	struct py_grammar* g0, * g;

	fp = fopen(filename, "r");
	if(fp == NULL) {
		perror(filename);
		exit(1);
	}
	g0 = &py_meta_grammar;
	n = NULL;
	py_parse_file(fp, filename, g0, g0->start, NULL, NULL, &n);
	fclose(fp);
	if(n == NULL) {
		fprintf(stderr, "Parsing error.\n");
		exit(1);
	}
	g = py_grammar_gen(n);
	if(g == NULL) {
		fprintf(stderr, "Bad grammar.\n");
		exit(1);
	}
	return g;
}

void py_fatal(const char* msg) {
	fprintf(stderr, "pgen: FATAL ERROR: %s\n", msg);
	exit(1);
}

/* TODO: check for duplicate definitions of names (instead of py_fatal err) */
