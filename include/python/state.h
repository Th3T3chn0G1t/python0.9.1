/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_STATE_H
#define PY_STATE_H

/* Consolidated Python interpreter state objects. */

#include <python/result.h>
#include <python/object.h>

/* Main interpreter environment. */
struct py {
	/* Type info table. */
	struct py_type_info types[PY_TYPE_MAX];
};

/* Current running environment. */
struct py_env {
	/* Owning interpreter. */
	struct py* py;

	/* Current function frame. */
	struct py_frame* current;

	/*
	 * NOTE: For the purposes of AGA -- we're not really expecting any
	 * 		 Non-native shared modules to exist in the interpreter state so
	 * 		 Having this be managed per-environment is fine.
	 * TODO: Split lookup between per-env state and the main interpreter state
	 * 		 For builtins.
	 */
	/* Modules dict. */
	struct py_object* modules;
};

enum py_result py_new(struct py*);
enum py_result py_delete(struct py*);

enum py_result py_env_new(struct py*, struct py_env*);
enum py_result py_env_delete(struct py_env*);

#endif
