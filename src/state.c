/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#include <python/state.h>

#include <python/dictobject.h>

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

	/*
	 * TODO: Make basic objects not propogate exceptions -- instead results
	 * 		 Then caller propogates. Do in tandem with putting typechecking
	 * 		 Responsibility on caller to avoid redundant rechecks.
	 */
	if(!(env->modules = py_dict_new())) return PY_RESULT_OOM;

	return PY_RESULT_OK;
}

enum py_result py_env_delete(struct py_env* env) {
	(void) env;
	return PY_RESULT_OK;
}
