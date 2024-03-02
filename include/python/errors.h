/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Error handling definitions */

#ifndef PY_ERRORS_H
#define PY_ERRORS_H

#include <python/object.h>

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
#define PY_KEY_ERROR py_runtime_error
#define PY_ZERO_DIVISION_ERROR py_runtime_error
#define PY_OVERFLOW_ERROR py_runtime_error

void py_error_set(struct py_object*);
void py_error_set_value(struct py_object*, struct py_object*);
void py_error_set_string(struct py_object*, const char*);
int py_error_occurred(void);
void py_error_get(struct py_object**, struct py_object**);
void py_error_clear(void);

/* Convenience functions */
int py_error_set_badarg(void);
struct py_object* py_error_set_nomem(void);
struct py_object* py_error_set_errno(struct py_object*);
void py_error_set_input(int);
void py_error_set_badcall(void);

/* TODO: Hook this. */
void py_fatal(const char*);

#endif
