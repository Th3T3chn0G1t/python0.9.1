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

#include <python/moduleobject.h>
#include <python/dictobject.h>
#include <python/listobject.h>
#include <python/stringobject.h>

/* TODO: This system needs some rework to clean up import control flow. */

/* TODO: Python global state. */
struct py_object* py_path;

struct py_object* py_path_new(const char* path) {
	static const char delim = ':';

	int i, n;
	const char* p;
	struct py_object* v, * w;

	n = 1;
	p = path;

	while((p = strchr(p, delim)) != NULL) {
		n++;
		p++;
	}

	if(!(v = py_list_new(n))) return NULL;

	for(i = 0;; i++) {
		if(!(p = strchr(path, delim))) p = strchr(path, '\0');

		w = py_string_new_size(path, (unsigned) (p - path));
		if(w == NULL) {
			py_object_decref(v);
			return NULL;
		}

		py_list_set(v, i, w);
		if(*p == '\0') break;

		path = p + 1;
	}

	return v;
}

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

/* TODO: No buffer overflow checks! */
static FILE* py_open_module(
		const char* name, const char* suffix, char* namebuf) {

	FILE* fp;

	if(py_path == NULL || !(py_path->type == PY_TYPE_LIST)) {
		strcpy(namebuf, name);
		strcat(namebuf, suffix);
		fp = py_open_r(namebuf);
	}
	else {
		unsigned npath = py_varobject_size(py_path);
		unsigned i;

		fp = NULL;
		for(i = 0; i < npath; i++) {
			struct py_object* v = py_list_get(py_path, i);
			unsigned len;

			if(!(v->type == PY_TYPE_STRING)) continue;

			strcpy(namebuf, py_string_get(v));
			len = py_varobject_size(v);

			/* TODO: Play nice with Windows file separators? */
			if(len > 0 && namebuf[len - 1] != '/') namebuf[len++] = '/';

			strcpy(namebuf + len, name);
			strcat(namebuf, suffix);

			fp = py_open_r(namebuf);
			if(fp != NULL) break;
		}
	}

	return fp;
}

static struct py_object* py_get_module(
		struct py_env* env, struct py_object* m, const char* name,
		struct py_object** ret) {

	struct py_object* d;
	struct py_node* n;
	FILE* fp;
	int err;

	/* TODO: This is not playing nice with buffers. */
	char namebuf[256];

	if(!(fp = py_open_module(name, ".py", namebuf))) {
		if(!m) py_error_set_string(py_name_error, name);
		else py_error_set_string(py_runtime_error, "no module source file");

		return NULL;
	}

	err = py_parse_file(
			fp, namebuf, &py_grammar, PY_GRAMMAR_FILE_INPUT, 0, 0, &n);
	py_close(fp);

	if(err != PY_RESULT_DONE) {
		py_error_set_input(err);
		return NULL;
	}

	if(!m) {
		if(!(m = py_module_add(env, name))) {
			py_tree_delete(n);
			return NULL;
		}

		*ret = m;
	}

	d = ((struct py_module*) m)->attr;

	return py_tree_run(env, n, namebuf, d, d);
}

struct py_object* py_import_module(struct py_env* env, const char* name) {
	struct py_object* m;
	struct py_object* v;

	if(!(m = py_dict_lookup(env->modules, name))) {
		if(!(v = py_get_module(env, NULL, name, &m))) return NULL;

		py_object_decref(v);
	}

	return m;
}

/* TODO: Shouldn't this be in dictobject? */
static void py_dict_clear(struct py_object* d) {
	unsigned i;

	for(i = 0; i < py_dict_size(d); ++i) {
		const char* k = py_dict_get_key(d, i);
		if(k != NULL) (void) py_dict_remove(d, k);
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
