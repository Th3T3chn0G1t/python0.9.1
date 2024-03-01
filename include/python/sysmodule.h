/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* System module interface */

#ifndef PY_SYSMODULE_H
#define PY_SYSMODULE_H

#include <python/object.h>

void py_system_init(void);
void py_system_done(void);

struct py_object* py_system_get(char*);
FILE* py_system_get_file(char*, FILE*);
int py_system_set(char*, struct py_object*);
void py_system_set_argv(int, char**);
void py_system_set_path(const char*);

#endif
