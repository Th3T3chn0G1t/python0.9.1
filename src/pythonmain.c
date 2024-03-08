/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Python interpreter main program */

#include <python/grammar.h>
#include <python/node.h>
#include <python/compile.h>
#include <python/ceval.h>

struct py_object* py_tree_run(
		struct py_node* n, char* filename, struct py_object* globals,
		struct py_object* locals) {

	if(globals == NULL) {
		globals = py_get_globals();
		if(locals == NULL) locals = py_get_locals();
	}
	else if(locals == NULL) locals = globals;

	return py_tree_eval(n, filename, globals, locals);
}

struct py_object* py_tree_eval(
		struct py_node* n, const char* filename, struct py_object* globals,
		struct py_object* locals) {

	struct py_code* co;
	struct py_object* v;

	co = py_compile(n, filename);
	py_tree_delete(n);
	if(co == NULL) return NULL;

	v = py_code_eval(co, globals, locals, (struct py_object*) NULL);
	py_object_decref(co);

	return v;
}

/*
 * XXX WISH LIST
 * - possible new types:
 * 		- iterator (for range, keys, ...)
 * - improve interpreter error handling, e.g., true tracebacks
 * - save precompiled modules on file?
 * - fork threads, locking
 * - allow syntax extensions
 */

/* "Floccinaucinihilipilification" */
