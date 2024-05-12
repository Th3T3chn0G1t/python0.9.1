/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback implementation */

#include <python/std.h>
#include <python/compile.h>
#include <python/traceback.h>
#include <python/errors.h>
#include <python/env.h>

#include <python/stringobject.h>

/* TODO: Python global state. */
static struct py_traceback* py_traceback_current = NULL;

static struct py_traceback* py_traceback_new_frame(
		struct py_traceback* next, struct py_frame* frame, unsigned lineno) {

	struct py_traceback* tb;

	if((next != NULL && !(next->ob.type == PY_TYPE_TRACEBACK)) ||
		frame == NULL || !(frame->ob.type == PY_TYPE_FRAME)) {

		py_error_set_badcall();
		return NULL;
	}

	tb = py_object_new(PY_TYPE_TRACEBACK);
	if(tb == NULL) return NULL;

	py_object_incref(next);
	tb->next = next;
	py_object_incref(frame);
	tb->frame = frame;

	tb->lineno = lineno;

	return tb;
}

int py_traceback_new(struct py_frame* frame, unsigned lineno) {
	struct py_traceback* tb;

	tb = py_traceback_new_frame(py_traceback_current, frame, lineno);
	if(tb == NULL) return -1;

	py_object_decref(py_traceback_current);
	py_traceback_current = tb;

	return 0;
}

struct py_object* py_traceback_get(void) {
	struct py_object* v;

	v = (struct py_object*) py_traceback_current; /* TODO: decref current? */
	py_traceback_current = NULL;

	return v;
}

/* TODO: Questionable stream EH. */
static void py_traceback_print_line(
		FILE* fp, const char* filename, unsigned lineno) {

	FILE* xfp;
	/* TODO: Suspicious buffer. */
	char buf[1024];
	unsigned i;

	/* TODO: Debug info to not require sources for tb. */
	xfp = py_open_r(filename);
	if(xfp == NULL) {
		/* TODO: Better EH? */
		fprintf(fp, "    (cannot open \"%s\")\n", filename);
		return;
	}

	for(i = 0; i < lineno; i++) {
		if(fgets(buf, sizeof(buf), xfp) == NULL) break;
	}

	if(i == lineno) {
		char* p = buf;
		while(*p == ' ' || *p == '\t') p++;

		fprintf(fp, "    %s", p);
		if(strchr(p, '\n') == NULL) fprintf(fp, "\n");
	}

	py_close(xfp); /* TODO: Reopening file every tb? */
}

static void py_traceback_print_impl(struct py_traceback* tb, FILE* fp) {
	while(tb != NULL) {
		fprintf(
				fp, "  File \"%s\", line %d\n",
				py_string_get(tb->frame->code->filename), tb->lineno);

		py_traceback_print_line(
				fp, py_string_get(tb->frame->code->filename),
				tb->lineno);

		tb = tb->next;
	}
}

int py_traceback_print(struct py_object* v, FILE* fp) {
	if(v == NULL) return 0;

	if(!(v->type == PY_TYPE_TRACEBACK)) {
		py_error_set_badcall();
		return -1;
	}

	py_traceback_print_impl((struct py_traceback*) v, fp);

	return 0;
}

void py_traceback_dealloc(struct py_object* op) {
	struct py_traceback* tb = (struct py_traceback*) op;

	py_object_decref(tb->next);
	py_object_decref(tb->frame);
}
