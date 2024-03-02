/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Python interpreter main program */

#include <python/grammar.h>
#include <python/node.h>
#include <python/compile.h>
#include <python/ceval.h>

struct py_object* py_tree_run(n, filename, globals, locals)struct py_node* n;
														   char* filename;
		/*dict*/struct py_object* globals, * locals;
{
	if(globals == NULL) {
		globals = py_get_globals();
		if(locals == NULL) {
			locals = py_get_locals();
		}
	}
	else {
		if(locals == NULL) {
			locals = globals;
		}
	}
	return py_tree_eval(n, filename, globals, locals);
}

struct py_object* py_tree_eval(n, filename, globals, locals)struct py_node* n;
															char* filename;
															struct py_object* globals;
															struct py_object* locals;
{
	struct py_code* co;
	struct py_object* v;
	co = py_compile(n, filename);
	py_tree_delete(n);
	if(co == NULL) {
		return NULL;
	}
	v = py_code_eval(co, globals, locals, (struct py_object*) NULL);
	py_object_decref(co);
	return v;
}

/* Print py_fatal error message and abort */

void py_fatal(msg)char* msg;
{
	/* TODO: Hook all random python/libwww prints for sinking. */
	fprintf(stderr, "Fatal error: %s\n", msg);
	abort();
}

/* TODO: Fix trace refs. */

/*
void finaloutput(void) {
#ifdef PY_TRACE_REFS
	if (!askyesno("Print left references?"))
			return;
	py_print_refs(stderr);
#endif
}
 */

/* Ask a yes/no question */

int askyesno(char* prompt);

int askyesno(char* prompt) {
	char buf[256];

	printf("%s [ny] ", prompt);
	if(fgets(buf, sizeof(buf), stdin) == NULL) {
		return 0;
	}
	return buf[0] == 'y' || buf[0] == 'Y';
}

#ifdef THINK_C_3_0

/* Check for file descriptor connected to interactive device.
   Pretend that stdin is always interactive, other files never. */

int
isatty(fd)
	   int fd;
{
	   return fd == fileno(stdin);
}

#endif

/*     XXX WISH LIST

	   - possible new types:
			   - iterator (for range, keys, ...)
	   - improve interpreter error handling, e.g., true tracebacks
	   - save precompiled modules on file?
	   - fork threads, locking
	   - allow syntax extensions
*/

/* "Floccinaucinihilipilification" */
