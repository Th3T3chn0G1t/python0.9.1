/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module definition and import implementation */

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

/* TODO: Python global state. */
static struct py_object* modules;
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
	v = py_list_new(n);
	if(v == NULL) {
		return NULL;
	}
	for(i = 0;; i++) {
		p = strchr(path, delim);
		if(p == NULL) {
			p = strchr(path, '\0');
		} /* End of string */
		w = py_string_new_size(path, (int) (p - path));
		if(w == NULL) {
			py_object_decref(v);
			return NULL;
		}
		py_list_set(v, i, w);
		if(*p == '\0') {
			break;
		}
		path = p + 1;
	}
	return v;
}

/* Initialization */

void py_import_init(void) {
	if((modules = py_dict_new()) == NULL) {
		py_fatal("no mem for dictionary of modules");
	}
}

struct py_object* py_add_module(name)char* name;
{
	struct py_object* m;
	if((m = py_dict_lookup(modules, name)) != NULL && py_is_module(m)) {
		return m;
	}
	m = py_module_new(name);
	if(m == NULL) {
		return NULL;
	}
	if(py_dict_insert(modules, name, m) != 0) {
		py_object_decref(m);
		return NULL;
	}
	py_object_decref(m); /* Yes, it still exists, in modules! */
	return m;
}

static FILE* open_module(name, suffix, namebuf)char* name;
											   char* suffix;
											   char* namebuf; /* XXX No buffer overflow checks! */
{
	FILE* fp;

	if(py_path == NULL || !py_is_list(py_path)) {
		strcpy(namebuf, name);
		strcat(namebuf, suffix);
		fp = pyopen_r(namebuf);
	}
	else {
		int npath = py_varobject_size(py_path);
		int i;
		fp = NULL;
		for(i = 0; i < npath; i++) {
			struct py_object* v = py_list_get(py_path, i);
			int len;
			if(!py_is_string(v)) {
				continue;
			}
			strcpy(namebuf, py_string_get_value(v));
			len = py_varobject_size(v);
			if(len > 0 && namebuf[len - 1] != '/') {
				namebuf[len++] = '/';
			}
			strcpy(namebuf + len, name);
			strcat(namebuf, suffix);
			fp = pyopen_r(namebuf);
			if(fp != NULL) {
				break;
			}
		}
	}
	return fp;
}

static struct py_object* get_module(m, name, m_ret)
/*module*/struct py_object* m;
		  char* name;
		  struct py_object** m_ret;
{
	struct py_object* d;
	FILE* fp;
	struct py_node* n;
	int err;
	char namebuf[256];

	fp = open_module(name, ".py", namebuf);
	if(fp == NULL) {
		if(m == NULL) {
			py_error_set_string(py_name_error, name);
		}
		else {
			py_error_set_string(py_runtime_error, "no module source file");
		}
		return NULL;
	}
	err = py_parse_file(fp, namebuf, &py_grammar, file_input, 0, 0, &n);
	pyclose(fp);
	if(err != PY_RESULT_DONE) {
		py_error_set_input(err);
		return NULL;
	}
	if(m == NULL) {
		m = py_add_module(name);
		if(m == NULL) {
			py_tree_delete(n);
			return NULL;
		}
		*m_ret = m;
	}
	d = py_module_get_dict(m);
	return py_tree_run(n, namebuf, d, d);
}

static struct py_object* load_module(name)char* name;
{
	struct py_object* m, * v;
	v = get_module((struct py_object*) NULL, name, &m);
	if(v == NULL) {
		return NULL;
	}
	py_object_decref(v);
	return m;
}

struct py_object* py_import_module(name)char* name;
{
	struct py_object* m;
	if((m = py_dict_lookup(modules, name)) == NULL) {
		m = load_module(name);
	}
	return m;
}

struct py_object* py_reload_module(m)struct py_object* m;
{
	if(m == NULL || !py_is_module(m)) {
		py_error_set_string(py_type_error, "reload() argument must be module");
		return NULL;
	}
	/* XXX Ought to check for builtin modules -- can't reload these... */
	return get_module(m, py_module_get_name(m), (struct py_object**) NULL);
}

static void cleardict(d)struct py_object* d;
{
	int i;
	for(i = py_dict_size(d); --i >= 0;) {
		char* k;
		k = py_dict_get_key(d, i);
		if(k != NULL) {
			(void) py_dict_remove(d, k);
		}
	}
}

void py_import_done(void) {
	if(modules != NULL) {
		int i;
		/* Explicitly erase all modules; this is the safest way
		   to get rid of at least *some* circular dependencies */
		for(i = py_dict_size(modules); --i >= 0;) {
			char* k;
			k = py_dict_get_key(modules, i);
			if(k != NULL) {
				struct py_object* m;
				m = py_dict_lookup(modules, k);
				if(m != NULL && py_is_module(m)) {
					struct py_object* d;
					d = py_module_get_dict(m);
					if(d != NULL && py_is_dict(d)) {
						cleardict(d);
					}
				}
			}
		}
		cleardict(modules);
	}
	py_object_decref(modules);
}
