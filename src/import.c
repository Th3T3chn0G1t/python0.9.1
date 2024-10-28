/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module definition and import implementation */

#include <python/state.h>
#include <python/env.h>
#include <python/node.h>
#include <python/token.h>
#include <python/graminit.h>
#include <python/import.h>
#include <python/result.h>
#include <python/parsetok.h>
#include <python/errors.h>
#include <python/grammar.h>
#include <python/pgen.h>

#include <python/object/module.h>
#include <python/object/dict.h>
#include <python/object/list.h>
#include <python/object/string.h>

/* TODO: This system needs some rework to clean up import control flow. */

struct py_object* py_module_add(struct py_env* env, const char* name) {
	struct py_object* m;

	/* We trust that the modules dictionary only ever holds moduleobjects. */
	if((m = py_dict_lookup(env->modules, name))) return m;

	if(!(m = py_module_new(name))) return NULL;

	if(py_dict_insert(env->modules, name, m)) {
		py_object_decref(m);
		return NULL;
	}

	py_object_decref(m); /* Yes, it still exists, in py_modules! */
	return m;
}

static struct py_object* py_get_module(
		struct py_env* env, const char* name, struct py_object** ret) {

	static const char suffix[] = ".py.raw";

	char buf[255 + 1] = { 0 };
	unsigned i;

	struct asys_stream* fp = 0;

	struct py_object* d;
	struct py_node* n;

	enum py_result res;

	for(i = 0; env->py->path[i]; ++i) {
		char* el = env->py->path[i];

		unsigned namlen = (unsigned) strlen(name);
		unsigned pathlen = (unsigned) strlen(el);
		unsigned sufflen = sizeof(suffix) - 1;

		if(pathlen + namlen + sufflen >= sizeof(buf)) continue;

		memcpy(buf, el, pathlen);
		memcpy(buf + pathlen, name, namlen);
		/* Adds appropriate null terminator. */
		memcpy(buf + pathlen + namlen, suffix, sizeof(suffix));

		/* TODO: Better EH. */
		if(py_open_r(buf, &fp)) {
			py_error_set_string(py_system_error, buf);
			return 0;
		}
	}

	if(!fp) {
		py_error_set_string(py_name_error, name);
		return NULL;
	}

	res = py_parse_file(fp, buf, &py_grammar, PY_GRAMMAR_FILE_INPUT, 0, 0, &n);

	if(res != PY_RESULT_DONE) {
		py_error_set_input(res);
		return NULL;
	}

	if(!(*ret = py_module_add(env, name))) {
		py_tree_delete(n);
		return NULL;
	}

	d = ((struct py_module*) *ret)->attr;

	return py_tree_run(env, n, buf, d, d);
}

struct py_object* py_import_module(struct py_env* env, const char* name) {
	struct py_object* m;
	struct py_object* v;

	if(!(m = py_dict_lookup(env->modules, name))) {
		if(!(v = py_get_module(env, name, &m))) return NULL;

		py_object_decref(v);
	}

	return m;
}

/* TODO: Shouldn't this be in dictobject? */
static void py_dict_clear(struct py_object* d) {
	unsigned i;

	for(i = 0; i < py_dict_size(d); ++i) {
		const char* k = py_dict_get_key(d, i);
		if(k) (void) py_dict_remove(d, k);
	}
}

void py_import_done(struct py_env* env) {
	if(env->modules != NULL) {
		unsigned i;

		/*
		 * Explicitly erase all py_modules; this is the safest way to get rid
		 * Of at least *some* circular dependencies.
		 */

		/* TODO: Make this more robust. */
		for(i = 0; i < py_dict_size(env->modules); ++i) {
			const char* k = py_dict_get_key(env->modules, i);

			/*
			 * TODO: Are all these checks neccesary when iterating through a
			 * 		 Dict?
			 */
			if(k) {
				struct py_object* m = py_dict_lookup(env->modules, k);

				if(m) {
					struct py_object* d = ((struct py_module*) m)->attr;

					/* TODO: Can a module have a null attr dict? */
					if(d) py_dict_clear(d);
				}
			}
		}

		py_dict_clear(env->modules);
	}
}
