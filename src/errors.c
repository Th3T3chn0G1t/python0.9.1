/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Error handling -- see also run.c */

/*
 * New error handling interface.
 *
 * The following problem exists (existed): methods of built-in modules
 * are called with 'self' and 'args' arguments, but without a context
 * argument, so they have no way to raise a specific exception.
 * The same is true for the object implementations: no context argument.
 * The old convention was to set 'errno' and to return NULL.
 * The caller (usually py_call_function() in eval.c) detects the NULL
 * return value and then calls puterrno(ctx) to turn the errno value
 * into a true exception. Problems with this approach are:
 * - it used standard errno values to indicate Python-specific errors,
 *   but this means that when such an error code is reported by a system
 *   call (e.g., in module posix), the user gets a confusing message
 * - errno is a global variable, which makes extensions to a multi-
 *   threading environment difficult; e.g., in IRIX, multi-threaded
 *   programs must use the function oserror() instead of looking in errno
 * - there is no portable way to add new error numbers for specic
 *   situations -- the value space for errno is reserved to the OS, yet
 *   the way to turn module-specific errors into a module-specific
 *   exception requires module-specific values for errno
 * - there is no way to add a more situation-specific message to an
 *   error.
 *
 * The new interface solves all these problems. To return an error, a
 * built-in function calls py_error_set(exception), py_error_set_value(exception,
 * value) or py_error_set_string(exception, string), and returns NULL. These
 * functions save the value for later use by puterrno(). To adapt this
 * scheme to a multi-threaded environment, only the implementation of
 * py_error_set_value() has to be changed.
 */

#include <python/std.h>
#include <python/result.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/stringobject.h>
#include <python/tupleobject.h>
#include <python/intobject.h>

/* Last exception stored by py_error_set_value() */

/* TODO: Python global state. */
static struct py_object* last_exception;
static struct py_object* last_exc_val;

void py_error_set_value(exception, value)struct py_object* exception;
										 struct py_object* value;
{
	PY_XDECREF(last_exception);
	PY_XINCREF(exception);
	last_exception = exception;

	PY_XDECREF(last_exc_val);
	PY_XINCREF(value);
	last_exc_val = value;
}

void py_error_set(exception)struct py_object* exception;
{
	py_error_set_value(exception, (struct py_object*) NULL);
}

void py_error_set_string(exception, string)struct py_object* exception;
										   const char* string;
{
	struct py_object* value = py_string_new(string);
	py_error_set_value(exception, value);
	PY_XDECREF(value);
}

int py_error_occurred() {
	return last_exception != NULL;
}

void py_error_get(p_exc, p_val)struct py_object** p_exc;
							   struct py_object** p_val;
{
	*p_exc = last_exception;
	last_exception = NULL;
	*p_val = last_exc_val;
	last_exc_val = NULL;
}

void py_error_clear() {
	PY_XDECREF(last_exception);
	last_exception = NULL;
	PY_XDECREF(last_exc_val);
	last_exc_val = NULL;
}

/* Convenience functions to set a type error exception and return 0 */

int py_error_set_badarg() {
	py_error_set_string(
			py_type_error, "illegal argument type for built-in operation");
	return 0;
}

struct py_object* py_error_set_nomem() {
	py_error_set(py_memory_error);
	return NULL;
}

struct py_object* py_error_set_errno(exc)struct py_object* exc;
{
	struct py_object* v = py_tuple_new(2);
	if(v != NULL) {
		py_tuple_set(v, 0, py_int_new((long) errno));
		py_tuple_set(v, 1, py_string_new(strerror(errno)));
	}
	py_error_set_value(exc, v);
	PY_XDECREF(v);
	return NULL;
}

void py_error_set_badcall() {
	py_error_set_string(py_system_error, "bad argument to internal function");
}

/* Set the error appropriate to the given input error code (see result.h) */

void py_error_set_input(err)int err;
{
	switch(err) {
		case PY_RESULT_DONE:
		case PY_RESULT_OK:break;
		case PY_RESULT_SYNTAX:
			py_error_set_string(
					py_runtime_error, "syntax error");
			break;
		case PY_RESULT_TOKEN:
			py_error_set_string(
					py_runtime_error, "illegal token");
			break;
		case PY_RESULT_INTERRUPT:py_error_set(py_interrupt_error);
			break;
		case PY_RESULT_OOM:py_error_set_nomem();
			break;
		case PY_RESULT_EOF:py_error_set(py_eof_error);
			break;
		default:py_error_set_string(py_runtime_error, "unknown input error");
			break;
	}
}
