/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser generator main program */

/* This expects a filename containing the grammar as argv[1] (UNIX)
   or asks the console for such a file name (THINK C).
   It writes its output on two files in the current directory:
   - "graminit.c" gets the grammar as a bunch of initialized data
   - "graminit.h" gets the grammar's non-terminals as #defines.
   Error messages and status info during the generation process are
   written to stdout, or sometimes to stderr. */

#include <stdlib.h>

#include <python/grammar.h>
#include <python/node.h>
#include <python/parsetok.h>
#include <python/pgen.h>

int debugging;

/* Forward */
struct py_grammar* getgrammar(char* filename);

#ifdef THINK_C
int main PROTO((int, char **));
char *askfile PROTO((void));
#endif

int main(argc, argv)int argc;
					char** argv;
{
	struct py_grammar* g;
	FILE* fp;
	char* filename;

	if(argc != 4) {
		fprintf(stderr, "usage: %s grammar outsource outheader\n", argv[0]);
		exit(2);
	}
	filename = argv[1];
	g = getgrammar(filename);
	fp = fopen(argv[2], "w");
	if(fp == NULL) {
		perror("graminit.c");
		exit(1);
	}
	fprintf(stderr, "Writing graminit.c ...\n");
	py_grammar_print(g, fp);
	fclose(fp);
	fp = fopen(argv[3], "w");
	if(fp == NULL) {
		perror("graminit.h");
		exit(1);
	}
	fprintf(stderr, "Writing graminit.h ...\n");
	py_grammar_print_nonterminals(g, fp);
	fclose(fp);
	exit(0);
}

struct py_grammar* getgrammar(filename)char* filename;
{
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
	py_parse_file(fp, filename, g0, g0->start, (char*) NULL, (char*) NULL, &n);
	fclose(fp);
	if(n == NULL) {
		fprintf(stderr, "Parsing error.\n");
		exit(1);
	}
	g = pgen(n);
	if(g == NULL) {
		fprintf(stderr, "Bad grammar.\n");
		exit(1);
	}
	return g;
}

#ifdef THINK_C
char *
askfile()
{
	   char buf[256];
	   static char name[256];
	   printf("Input file name: ");
	   if (fgets(buf, sizeof buf, stdin) == NULL) {
			   printf("EOF\n");
			   exit(1);
	   }
	   /* XXX The (unsigned char *) case is needed by THINK C 3.0 */
	   if (sscanf(/*(unsigned char *)*/buf, " %s ", name) != 1) {
			   printf("No file\n");
			   exit(1);
	   }
	   return name;
}
#endif

void py_fatal(msg)char* msg;
{
	fprintf(stderr, "pgen: FATAL ERROR: %s\n", msg);
	exit(1);
}

/* XXX TO DO:
   - check for duplicate definitions of names (instead of py_fatal err)
*/
