/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* POSIX module implementation */

/* TODO: How this handles includes and feature tests is terrible. */

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _MSC_VER
# include <sys/time.h>
#endif

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/stringobject.h>
#include <python/intobject.h>
#include <python/tupleobject.h>
#include <python/listobject.h>
#include <python/dictobject.h>
#include <python/moduleobject.h>

#ifndef _WIN32
/* Return a dictionary corresponding to the POSIX environment table */

extern char** environ;

static struct py_object* convertenviron() {
	struct py_object* d;
	char** e;
	d = py_dict_new();
	if(d == NULL) {
		return NULL;
	}
	if(environ == NULL) {
		return d;
	}
	/* XXX This part ignores errors */
	for(e = environ; *e != NULL; e++) {
		struct py_object* v;
		char* p = strchr(*e, '=');
		if(p == NULL) {
			continue;
		}
		v = py_string_new(p + 1);
		if(v == NULL) {
			continue;
		}
		*p = '\0';
		(void) py_dict_insert(d, *e, v);
		*p = '=';
		PY_DECREF(v);
	}
	return d;
}

#endif

static struct py_object* PosixError; /* Exception posix.error */

/* Set a POSIX-specific error from errno, and return NULL */

static struct py_object* posix_error() {
	return py_error_set_errno(PosixError);
}


/* POSIX generic methods */

static struct py_object* posix_1str(args, func)struct py_object* args;
									 int (* func)(const char*);
{
	struct py_object* path1;
	if(!py_arg_str(args, &path1)) {
		return NULL;
	}
	if((*func)(
			py_string_get_value(path1)) < 0) {
		return

				posix_error();
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_object* posix_2str(args, func)struct py_object* args;
									 int (* func)(const char*, const char*);
{
	struct py_object* path1, * path2;
	if(!py_arg_str_str(args, &path1, &path2)) {
		return NULL;
	}
	if((*func)(
			py_string_get_value(path1), py_string_get_value(path2)) < 0) {
		return

				posix_error();
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_object* posix_strint(args, func)struct py_object* args;
									   int (* func)(const char*, int);
{
	struct py_object* path1;
	int i;
	if(!py_arg_str_int(args, &path1, &i)) {
		return NULL;
	}
	if((*func)(
			py_string_get_value(path1), i) < 0) {
		return

				posix_error();
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_object* posix_do_stat(self, args, statfunc)struct py_object* self;
												  struct py_object* args;
												  int (* statfunc)(
														  const char*,
														  struct stat*);
{
	struct stat st;
	struct py_object* path;
	struct py_object* v;

	(void) self;

	if(!py_arg_str(args, &path)) {
		return NULL;
	}
	if((*statfunc)(
			py_string_get_value(path), &st) != 0) {
		return

				posix_error();
	}

	v = py_tuple_new(10);
	if(v == NULL) {
		return NULL;
	}
#define SET(i, st_member) py_tuple_set(v, i, py_int_new((long)st.st_member))
	SET(0, st_mode);
	SET(1, st_ino);
	SET(2, st_dev);
	SET(3, st_nlink);
	SET(4, st_uid);
	SET(5, st_gid);
	SET(6, st_size);
	SET(7, st_atime);
	SET(8, st_mtime);
	SET(9, st_ctime);
#undef SET
	if(

			py_error_occurred()

			) {
		PY_DECREF(v);
		return NULL;
	}
	return v;
}


/* POSIX methods */

static struct py_object* posix_chdir(self, args)struct py_object* self;
									  struct py_object* args;
{
	int chdir(const char*);

	(void) self;

	return posix_1str(args, chdir);
}

static struct py_object* posix_chmod(self, args)struct py_object* self;
									  struct py_object* args;
{
#ifdef _WIN32
	int chmod(const char*, int);
#else
	int chmod(const char* mode_t);
#endif
	(void) self;

	return posix_strint(args, chmod);
}

static struct py_object* posix_getcwd(self, args)struct py_object* self;
									   struct py_object* args;
{
	char buf[1026];
	char* getcwd(char*, int);

	(void) self;

	if(!py_arg_none(args)) {
		return NULL;
	}
	if(getcwd(buf, sizeof buf) == NULL) {
		return posix_error();
	}
	return py_string_new(buf);
}

#ifndef _WIN32
static struct py_object*
posix_link(self, args)
	   struct py_object*self;
	   struct py_object*args;
{
	(void) self;

	int link (const char* const char *);
	return posix_2str(args, link);
}
#endif

#ifndef _MSC_VER
static struct py_object* posix_listdir(self, args)struct py_object* self;
										struct py_object* args;
{
	struct py_object* name, * d, * v;
	DIR* dirp;
	struct dirent* ep;

	(void) self;

	if(!py_arg_str(args, &name)) {
		return NULL;
	}
	if((dirp = opendir(py_string_get_value(name))) == NULL) {
		return posix_error();
	}
	if((d = py_list_new(0)) == NULL) {
		closedir(dirp);
		return NULL;
	}
	while((ep = readdir(dirp)) != NULL) {
		v = py_string_new(ep->d_name);
		if(v == NULL) {
			PY_DECREF(d);
			d = NULL;
			break;
		}
		if(py_list_add(d, v) != 0) {
			PY_DECREF(v);
			PY_DECREF(d);
			d = NULL;
			break;
		}
		PY_DECREF(v);
	}
	closedir(dirp);
	return d;
}
#endif

#ifdef _WIN32

static int winmkdir(const char* path, int mode) {
	(void) mode;
	return mkdir(path);
}

#endif

static struct py_object* posix_mkdir(self, args)struct py_object* self;
									  struct py_object* args;
{
#ifndef _WIN32
	int mkdir (const char* mode_t);

	(void) self;

	return posix_strint(args, mkdir);
#else
	(void) self;

	return posix_strint(args, winmkdir);
#endif
}

static struct py_object* posix_rename(self, args)struct py_object* self;
									   struct py_object* args;
{
	int rename(const char*, const char*);

	(void) self;

	return posix_2str(args, rename);
}

static struct py_object* posix_rmdir(self, args)struct py_object* self;
									  struct py_object* args;
{
	int rmdir(const char*);

	(void) self;

	return posix_1str(args, rmdir);
}

static struct py_object* posix_stat(self, args)struct py_object* self;
									 struct py_object* args;
{
	int stat(const char*, struct stat*);

	(void) self;

	return posix_do_stat(self, args, stat);
}

static struct py_object* posix_system(self, args)struct py_object* self;
									   struct py_object* args;
{
	struct py_object* command;
	int sts;

	(void) self;

	if(!py_arg_str(args, &command)) {
		return NULL;
	}
	sts = system(py_string_get_value(command));
	return py_int_new((long) sts);
}

static struct py_object* posix_umask(self, args)struct py_object* self;
									  struct py_object* args;
{
	int i;

	(void) self;

	if(!py_arg_int(args, &i)) {
		return NULL;
	}
	i = umask(i);
	if(i < 0) {
		return posix_error();
	}
	return py_int_new((long) i);
}

static struct py_object* posix_unlink(self, args)struct py_object* self;
									   struct py_object* args;
{
	int unlink(const char*);

	(void) self;

	return posix_1str(args, unlink);
}

#ifndef _WIN32
static struct py_object*
posix_utimes(self, args)
	   struct py_object*self;
	   struct py_object*args;
{
	   struct py_object*path;
	   struct timeval tv[2];

		(void) self;

	   if (args == NULL || !py_is_tuple(args) || py_tuple_size(args) != 2) {
			   py_error_set_badarg();
			   return NULL;
	   }
	   if (!py_arg_str(py_tuple_get(args, 0), &path) ||
							   !py_arg_long_long(py_tuple_get(args, 1),
									   &tv[0].tv_sec, &tv[1].tv_sec))
			   return NULL;
	   tv[0].tv_usec = tv[1].tv_usec = 0;
	   if (utimes(py_string_get_value(path), tv) < 0)
			   return posix_error();
	   PY_INCREF(PY_NONE);
	   return PY_NONE;
}
#endif

#ifndef _WIN32

static struct py_object*
posix_lstat(self, args)
	   struct py_object*self;
	   struct py_object*args;
{
	   int lstat (const char* struct stat *);

		(void) self;

	   return posix_do_stat(self, args, lstat);
}

static struct py_object*
posix_readlink(self, args)
	   struct py_object*self;
	   struct py_object*args;
{
	   char buf[1024]; /* XXX Should use MAXPATHLEN */
	   struct py_object*path;
	   int n;
	   if (!py_arg_str(args, &path))
			   return NULL;
	   n = readlink(py_string_get_value(path), buf, sizeof buf);
	   if (n < 0)
			   return posix_error();
	   return py_string_new_size(buf, n);
}

static struct py_object*
posix_symlink(self, args)
	   struct py_object*self;
	   struct py_object*args;
{
	   int symlink (const char* const char *);
	   return posix_2str(args, symlink);
}

#endif /* NO_LSTAT */


static struct py_methodlist posix_methods[] = {
		{ "chdir", posix_chdir }, { "chmod", posix_chmod },
		{ "getcwd", posix_getcwd },
#ifndef _WIN32
		{"link",      posix_link},
#endif
#ifndef _MSC_VER
		{ "listdir", posix_listdir },
#endif
		{ "mkdir", posix_mkdir }, { "rename", posix_rename },
		{ "rmdir", posix_rmdir }, { "stat", posix_stat },
		{ "system", posix_system }, { "umask", posix_umask },
		{ "unlink", posix_unlink },
#ifndef _WIN32
		{"utimes",    posix_utimes},
		{"lstat",     posix_lstat},
		{"readlink",  posix_readlink},
		{"symlink",   posix_symlink},
#endif
		{ NULL, NULL }            /* Sentinel */
};


void initposix() {
	struct py_object* m, * d;

	m = py_module_init("posix", posix_methods);
	d = py_module_get_dict(m);

	/* Initialize posix.environ dictionary */
#ifndef _WIN32
	v = convertenviron();
	if(v == NULL || py_dict_insert(d, "environ", v) != 0) {
		py_fatal("can't define posix.environ");
	}
	PY_DECREF(v);
#endif

	/* Initialize posix.error exception */
	PosixError = py_string_new("posix.error");
	if(PosixError == NULL || py_dict_insert(d, "error", PosixError) != 0) {
		py_fatal("can't define posix.error");
	}
}
