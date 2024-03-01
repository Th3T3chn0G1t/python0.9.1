/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* File object implementation */

/*
 * XXX This should become a built-in module 'io'. It should support more
 * functionality, better exception handling for invalid calls, etc.
 * (Especially reading on a write-only file or vice versa!)
 * It should also cooperate with posix to support popen(), which should
 * share most code but have a special close function.
 */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/fileobject.h>
#include <python/stringobject.h>
#include <python/intobject.h>
#include <python/methodobject.h>

typedef struct {
	PY_OB_SEQ
	FILE* f_fp;
	struct py_object* f_name;
	struct py_object* f_mode;
	/* XXX Should move the 'need space' on printing flag here */
} fileobject;

FILE* py_file_get(f)struct py_object* f;
{
	if(!py_is_file(f)) {
		py_error_set_badcall();
		return NULL;
	}
	return ((fileobject*) f)->f_fp;
}

struct py_object* py_openfile_new(fp, name, mode)FILE* fp;
										 char* name;
										 char* mode;
{
	fileobject* f = py_object_new(&py_file_type);
	if(f == NULL) {
		return NULL;
	}
	f->f_fp = NULL;
	f->f_name = py_string_new(name);
	f->f_mode = py_string_new(mode);
	if(f->f_name == NULL || f->f_mode == NULL) {
		PY_DECREF(f);
		return NULL;
	}
	f->f_fp = fp;
	return (struct py_object*) f;
}

struct py_object* py_file_new(name, mode)char* name, * mode;
{
	fileobject* f;

	f = (fileobject*) py_openfile_new((FILE*) NULL, name, mode);
	if(f == NULL) {
		return NULL;
	}
#ifdef THINK_C
	if (*mode == '*') {
			FILE *fopenRF();
			f->f_fp = fopenRF(name, mode+1);
	}
	else
#endif
	f->f_fp = fopen(name, mode);
	if(f->f_fp == NULL) {
		py_error_set_errno(py_runtime_error);
		PY_DECREF(f);
		return NULL;
	}
	return (struct py_object*) f;
}

/* Methods */

static void file_dealloc(f)fileobject* f;
{
	if(f->f_fp != NULL) {
		fclose(f->f_fp);
	}
	if(f->f_name != NULL)
		PY_DECREF(f->f_name);
	if(f->f_mode != NULL)
		PY_DECREF(f->f_mode);
	free(f);
}

static void file_print(f, fp, flags)fileobject* f;
									FILE* fp;
									int flags;
{
	fprintf(fp, "<%s file ", f->f_fp == NULL ? "closed" : "open");
	py_object_print(f->f_name, fp, flags);
	fprintf(fp, ", mode ");
	py_object_print(f->f_mode, fp, flags);
	fprintf(fp, ">");
}

static struct py_object* file_repr(f)fileobject* f;
{
	char buf[300];
	/* XXX This differs from file_print if the filename contains
	   quotes or other funny characters. */
	sprintf(
			buf, "<%s file '%.256s', mode '%.10s'>",
			f->f_fp == NULL ? "closed" : "open", py_string_get_value(f->f_name),
			py_string_get_value(f->f_mode));
	return py_string_new(buf);
}

static struct py_object* file_close(f, args)fileobject* f;
								  struct py_object* args;
{
	if(args != NULL) {
		py_error_set_badarg();
		return NULL;
	}
	if(f->f_fp != NULL) {
		fclose(f->f_fp);
		f->f_fp = NULL;
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_object* file_read(f, args)fileobject* f;
								 struct py_object* args;
{
	int n;
	struct py_object* v;
	if(f->f_fp == NULL) {
		py_error_set_badarg();
		return NULL;
	}
	if(args == NULL || !py_is_int(args)) {
		py_error_set_badarg();
		return NULL;
	}
	n = py_int_get(args);
	if(n < 0) {
		py_error_set_badarg();
		return NULL;
	}
	v = py_string_new_size((char*) NULL, n);
	if(v == NULL) {
		return NULL;
	}
	n = fread(py_string_get_value(v), 1, n, f->f_fp);
	/* EOF is reported as an empty string */
	/* XXX should detect real I/O errors? */
	py_string_resize(&v, n);
	return v;
}

/* XXX Should this be unified with raw_input()? */

static struct py_object* file_readline(f, args)fileobject* f;
									 struct py_object* args;
{
	int n;
	struct py_object* v;
	if(f->f_fp == NULL) {
		py_error_set_badarg();
		return NULL;
	}
	if(args == NULL) {
		n = 10000; /* XXX should really be unlimited */
	}
	else if(py_is_int(args)) {
		n = py_int_get(args);
		if(n < 0) {
			py_error_set_badarg();
			return NULL;
		}
	}
	else {
		py_error_set_badarg();
		return NULL;
	}
	v = py_string_new_size((char*) NULL, n);
	if(v == NULL) {
		return NULL;
	}
#ifndef THINK_C_3_0
	/* XXX Think C 3.0 wrongly reads up to n characters... */
	n = n + 1;
#endif
	if(fgets(py_string_get_value(v), n, f->f_fp) == NULL) {
		/* EOF is reported as an empty string */
		/* XXX should detect real I/O errors? */
		n = 0;
	}
	else {
		n = strlen(py_string_get_value(v));
	}
	py_string_resize(&v, n);
	return v;
}

static struct py_object* file_write(f, args)fileobject* f;
								  struct py_object* args;
{
	int n, n2;
	if(f->f_fp == NULL) {
		py_error_set_badarg();
		return NULL;
	}
	if(args == NULL || !py_is_string(args)) {
		py_error_set_badarg();
		return NULL;
	}
	errno = 0;
	n2 = fwrite(py_string_get_value(args), 1, n = py_string_size(args), f->f_fp);
	if(n2 != n) {
		if(errno == 0)
			errno = EIO;
		py_error_set_errno(py_runtime_error);
		return NULL;
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_methodlist file_methods[] = {
		{ "write",    file_write },
		{ "read",     file_read },
		{ "readline", file_readline },
		{ "close",    file_close },
		{ NULL, NULL }           /* sentinel */
};

static struct py_object* file_getattr(f, name)fileobject* f;
									char* name;
{
	return py_methodlist_find(file_methods, (struct py_object*) f, name);
}

struct py_type py_file_type = {
		PY_OB_SEQ_INIT(&py_type_type) 0, "file", sizeof(fileobject), 0,
		file_dealloc,   /*dealloc*/
		file_print,     /*print*/
		file_getattr,   /*get_attr*/
		0,              /*set_attr*/
		0,              /*cmp*/
		file_repr,      /*repr*/
		0, 0, 0
};
