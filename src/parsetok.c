/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Parser-tokenizer link implementation */

#include <stdlib.h>

#include <python/tokenizer.h>
#include <python/node.h>
#include <python/grammar.h>
#include <python/parser.h>
#include <python/parsetok.h>
#include <python/result.h>
#include <python/token.h>

/* Forward */
static int parsetok(struct py_tokenizer*, struct py_grammar*, int, struct py_node**);

/* Parse input coming from a file. Return error code, print some errors. */

int py_parse_file(fp, filename, g, start, ps1, ps2, n_ret)FILE* fp;
													  char* filename;
													  struct py_grammar* g;
													  int start;
													  char* ps1, * ps2;
													  struct py_node** n_ret;
{
	struct py_tokenizer* tok = py_tokenizer_setup_file(fp, ps1, ps2);
	int ret;

	if(tok == NULL) {
		fprintf(stderr, "no mem for py_tokenizer_setup_file\n");
		return PY_RESULT_OOM;
	}
	ret = parsetok(tok, g, start, n_ret);
	if(ret == PY_RESULT_TOKEN || ret == PY_RESULT_SYNTAX) {
		char* p;
		fprintf(
				stderr, "Parsing error: file %s, line %d:\n", filename,
				tok->lineno);
		*tok->inp = '\0';
		if(tok->inp > tok->buf && tok->inp[-1] == '\n') {
			tok->inp[-1] = '\0';
		}
		fprintf(stderr, "%s\n", tok->buf);
		for(p = tok->buf; p < tok->cur; p++) {
			if(*p == '\t') {
				putc('\t', stderr);
			}
			else {
				putc(' ', stderr);
			}
		}
		fprintf(stderr, "^\n");
	}
	py_tokenizer_delete(tok);
	return ret;
}


/* Parse input coming from the given tokenizer structure.
   Return error code. */

static int parsetok(tok, g, start, n_ret)struct py_tokenizer* tok;
										 struct py_grammar* g;
										 int start;
										 struct py_node** n_ret;
{
	struct py_parser* ps;
	int ret;

	if((ps = py_parser_new(g, start)) == NULL) {
		fprintf(stderr, "no mem for new parser\n");
		return PY_RESULT_OOM;
	}

	for(;;) {
		char* a, * b;
		int type;
		int len;
		char* str;

		type = py_tokenizer_get(tok, &a, &b);
		if(type == PY_ERRORTOKEN) {
			ret = tok->done;
			break;
		}
		len = b - a;
		str = malloc((len + 1) * sizeof(char));
		if(str == NULL) {
			fprintf(stderr, "no mem for next token\n");
			ret = PY_RESULT_OOM;
			break;
		}
		strncpy(str, a < tok->buf ? tok->buf : a, len);
		str[len] = '\0';
		ret = py_parser_add(ps, (int) type, str, tok->lineno);
		if(ret != PY_RESULT_OK) {
			if(ret == PY_RESULT_DONE) {
				*n_ret = ps->tree;
				ps->tree = NULL;
			}
			else if(tok->lineno <= 1 && tok->done == PY_RESULT_EOF) {
				ret = PY_RESULT_EOF;
			}
			break;
		}
	}

	py_parser_delete(ps);
	return ret;
}
