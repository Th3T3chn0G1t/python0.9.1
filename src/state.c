/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#include <python/state.h>

enum py_result py_new(struct py* py) {
	(void) py;
	return PY_RESULT_OK;
}

enum py_result py_delete(struct py* py) {
	(void) py;
	return PY_RESULT_OK;
}

enum py_result py_env_new(struct py* py, struct py_env* env) {
	env->py = py;

	return PY_RESULT_OK;
}

enum py_result py_env_delete(struct py_env* env) {
	(void) env;
	return PY_RESULT_OK;
}
