/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Configurable Python configuration file */

#include <stdio.h>

#include <python/config.h>

/* Table of built-in modules.
   These are initialized when first imported. */

/* Standard modules */
void inittime();
void initmath();
void initposix();

struct py_initmod py_init_table[] = {
	/* Standard modules */
	{ "time", inittime },
	{ "math", initmath },
	{ "posix", initposix },
	{ 0, 0 }              /* Sentinel */
};
