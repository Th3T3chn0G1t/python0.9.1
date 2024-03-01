/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* System module */

/*
Various bits of information used by the interpreter are collected in
module 'sys'.
Function member:
- exit(sts): call (C, POSIX) exit(sts)
Data members:
- stdin, stdout, stderr: standard file objects
- modules: the table of modules (dictionary)
- path: module search path (list of strings)
- argv: script arguments (list of strings)
- ps1, ps2: optional primary and secondary prompts (strings)
*/

#include <python/std.h>
#include <python/sysmodule.h>
#include <python/import.h>
#include <python/errors.h>

#include <python/modsupport.h>

#include <python/moduleobject.h>
#include <python/fileobject.h>
#include <python/listobject.h>
#include <python/dictobject.h>
#include <python/stringobject.h>

static struct py_object* sysdict;
static struct py_object* sysin, * sysout, * syserr;

struct py_object* py_system_get(name)char* name;
{
	return py_dict_lookup(sysdict, name);
}

void py_system_done(void) {
	PY_DECREF(sysin);
	PY_DECREF(sysout);
	PY_DECREF(syserr);
	PY_DECREF(sysdict);
}

FILE* py_system_get_file(name, def)char* name;
								   FILE* def;
{
	FILE* fp = NULL;
	struct py_object* v = py_system_get(name);
	if(v != NULL) {
		fp = py_file_get(v);
	}
	if(fp == NULL) {
		fp = def;
	}
	return fp;
}

int py_system_set(name, v)char* name;
						  struct py_object* v;
{
	if(v == NULL) {
		return py_dict_remove(sysdict, name);
	}
	else {
		return py_dict_insert(sysdict, name, v);
	}
}

static struct py_object*
sys_exit(struct py_object* self, struct py_object* args) {
	int sts;

	(void) self;

	PY_DECREF(sysdict);
	if(!py_arg_int(args, &sts)) {
		return NULL;
	}
	exit(sts); /* Just in case */
	/* NOTREACHED */
}

static struct py_methodlist sys_methods[] = {
		{ "exit", sys_exit },
		{ NULL, NULL }           /* sentinel */
};

void py_system_init() {
	struct py_object* m = py_module_init("sys", sys_methods);
	sysdict = py_module_get_dict(m);
	PY_INCREF(sysdict);
	/* NB keep an extra ref to the std files to avoid closing them
	   when the user deletes them */
	/* XXX File objects should have a "don't close" flag instead */
	sysin = py_openfile_new(stdin, "<stdin>", "r");
	sysout = py_openfile_new(stdout, "<stdout>", "w");
	syserr = py_openfile_new(stderr, "<stderr>", "w");
	if(py_error_occurred()) {
		py_fatal("can't create sys.std* file objects");
	}
	py_dict_insert(sysdict, "stdin", sysin);
	py_dict_insert(sysdict, "stdout", sysout);
	py_dict_insert(sysdict, "stderr", syserr);
	py_dict_insert(sysdict, "modules", py_get_modules());
	if(py_error_occurred()) {
		py_fatal("can't insert sys.* objects in sys dict");
	}
}

static struct py_object* makepathobject(path, delim)char* path;
													int delim;
{
	int i, n;
	char* p;
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
			PY_DECREF(v);
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

void py_system_set_path(const char* path) {
	struct py_object* v;
	if((v = makepathobject(path, ':')) == NULL) {
		py_fatal("can't create sys.path");
	}
	if(py_system_set("path", v) != 0) {
		py_fatal("can't assign sys.path");
	}
	PY_DECREF(v);
}

static struct py_object* makeargvobject(argc, argv)int argc;
												   char** argv;
{
	struct py_object* av;
	if(argc < 0 || argv == NULL) {
		argc = 0;
	}
	av = py_list_new(argc);
	if(av != NULL) {
		int i;
		for(i = 0; i < argc; i++) {
			struct py_object* v = py_string_new(argv[i]);
			if(v == NULL) {
				PY_DECREF(av);
				av = NULL;
				break;
			}
			py_list_set(av, i, v);
		}
	}
	return av;
}

void py_system_set_argv(int argc, char** argv) {
	struct py_object* av = makeargvobject(argc, argv);
	if(av == NULL) {
		py_fatal("no mem for sys.argv");
	}
	if(py_system_set("argv", av) != 0) {
		py_fatal("can't assign sys.argv");
	}
	PY_DECREF(av);
}
