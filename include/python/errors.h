/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Error handling definitions */

#ifndef PY_ERRORS_H
#define PY_ERRORS_H

#include <python/object.h>
#include <python/result.h>

/* Predefined exceptions */

/* TODO: Python global state. */
extern struct py_object* py_runtime_error;
extern struct py_object* py_eof_error;
extern struct py_object* py_type_error;
extern struct py_object* py_memory_error;
extern struct py_object* py_name_error;
extern struct py_object* py_system_error;

/* Some more planned for the future */

#define PY_INDEX_ERROR py_runtime_error

void py_error_set(struct py_object*);
void py_error_set_value(struct py_object*, struct py_object*);
void py_error_set_string(struct py_object*, const char*);
int py_error_set_key(void);
int py_error_occurred(void);
void py_error_get(struct py_object**, struct py_object**);
void py_error_clear(void);

/* Convenience functions */
int py_error_set_badarg(void);
struct py_object* py_error_set_nomem(void);
void py_error_set_input(enum py_result);
void py_error_set_badcall(void);
void py_error_set_evalop(void);

void py_fatal(const char*);

#endif
