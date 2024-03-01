/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_CONFIG_H
#define PY_CONFIG_H

struct py_initmod {
	char* name;

	void (* func)(void);
};

/* TODO: Python global state. */
extern struct py_initmod py_init_table[];

#endif
