/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Token types */

#ifndef PY_TOKEN_H
#define PY_TOKEN_H

/* TODO: Use enum type for this and all uses. */
#define PY_ENDMARKER (0)
#define PY_NAME (1)
#define PY_NUMBER (2)
#define PY_STRING (3)
#define PY_NEWLINE (4)
#define PY_INDENT (5)
#define PY_DEDENT (6)
#define PY_LPAR (7)
#define PY_RPAR (8)
#define PY_LSQB (9)
#define PY_RSQB (10)
#define PY_COLON (11)
#define PY_COMMA (12)
#define PY_SEMI (13)
#define PY_PLUS (14)
#define PY_MINUS (15)
#define PY_STAR (16)
#define PY_SLASH (17)
#define PY_VBAR (18)
#define PY_AMPER (19)
#define PY_LESS (20)
#define PY_GREATER (21)
#define PY_EQUAL (22)
#define PY_DOT (23)
#define PY_PERCENT (24)
#define PY_BACKQUOTE (25)
#define PY_LBRACE (26)
#define PY_RBRACE (27)
#define PY_OP (28)
#define PY_ERRORTOKEN (29)
#define PY_N_TOKENS (30)

/* Special definitions for cooperation with parser */
#define PY_NONTERMINAL (256)

/* TODO: Python global state. */
extern char* py_token_names[]; /* Token names */

int py_token_char(int);

#endif
