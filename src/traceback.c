/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback implementation */

#include <python/std.h>
#include <python/compile.h>
#include <python/frameobject.h>
#include <python/traceback.h>
#include <python/structmember.h>
#include <python/errors.h>
#include <python/env.h>

typedef struct _tracebackobject {
	struct py_object ob;
	struct _tracebackobject* tb_next;
	struct py_frame* tb_frame;
	int tb_lasti;
	int tb_lineno;
} tracebackobject;

#define OFF(x) offsetof(tracebackobject, x)

static struct py_memberlist tb_memberlist[] = {
		{ "tb_next",   PY_TYPE_OBJECT, OFF(tb_next),   PY_READWRITE },
		{ "tb_frame",  PY_TYPE_OBJECT, OFF(tb_frame),  PY_READWRITE },
		{ "tb_lasti",  PY_TYPE_INT,    OFF(tb_lasti),  PY_READWRITE },
		{ "tb_lineno", PY_TYPE_INT,    OFF(tb_lineno), PY_READWRITE },
		{ NULL,        0, 0,                           0 }  /* Sentinel */
};

static struct py_object* tb_getattr(tb, name)tracebackobject* tb;
											 char* name;
{
	return py_memberlist_get((char*) tb, tb_memberlist, name);
}

static void tb_dealloc(tb)tracebackobject* tb;
{
	py_object_decref(tb->tb_next);
	py_object_decref(tb->tb_frame);
	free(tb);
}

static struct py_type Tracebacktype = {
		{ 1, &py_type_type, 0 }, "traceback", sizeof(tracebackobject),
		0, tb_dealloc,     /*dealloc*/
		0,              /*print*/
		tb_getattr,     /*get_attr*/
		0,              /*set_attr*/
		0,              /*cmp*/
		0,              /*repr*/
		0,              /*numbermethods*/
		0,              /*sequencemethods*/
		0,              /*mappingmethods*/
};

#define is_tracebackobject(v) \
	(((struct py_object*) (v))->type == &Tracebacktype)

static tracebackobject* newtracebackobject(next, frame, lasti, lineno)
		tracebackobject* next;
		struct py_frame* frame;
		int lasti, lineno;
{
	tracebackobject* tb;
	if((next != NULL && !is_tracebackobject(next)) || frame == NULL ||
	   !py_is_frame(frame)) {
		py_error_set_badcall();
		return NULL;
	}
	tb = py_object_new(&Tracebacktype);
	if(tb != NULL) {
		py_object_incref(next);
		tb->tb_next = next;
		py_object_incref(frame);
		tb->tb_frame = frame;
		tb->tb_lasti = lasti;
		tb->tb_lineno = lineno;
	}
	return tb;
}

static tracebackobject* tb_current = NULL;

int py_traceback_new(frame, lasti, lineno)struct py_frame* frame;
										  int lasti;
										  int lineno;
{
	tracebackobject* tb;
	tb = newtracebackobject(tb_current, frame, lasti, lineno);
	if(tb == NULL) {
		return -1;
	}
	py_object_decref(tb_current);
	tb_current = tb;
	return 0;
}

struct py_object* py_traceback_get() {
	struct py_object* v;
	v = (struct py_object*) tb_current;
	tb_current = NULL;
	return v;
}

int py_traceback_set(v)struct py_object* v;
{
	if(v != NULL && !is_tracebackobject(v)) {
		py_error_set_badcall();
		return -1;
	}
	py_object_decref(tb_current);
	py_object_incref(v);
	tb_current = (tracebackobject*) v;
	return 0;
}

static void tb_displayline(fp, filename, lineno)FILE* fp;
												char* filename;
												int lineno;
{
	FILE* xfp;
	char buf[1000];
	int i;
	if(filename[0] == '<' && filename[strlen(filename) - 1] == '>') {
		return;
	}
	xfp = pyopen_r(filename);
	if(xfp == NULL) {
		fprintf(fp, "    (cannot open \"%s\")\n", filename);
		return;
	}
	for(i = 0; i < lineno; i++) {
		if(fgets(buf, sizeof buf, xfp) == NULL) {
			break;
		}
	}
	if(i == lineno) {
		char* p = buf;
		while(*p == ' ' || *p == '\t')
			p++;
		fprintf(fp, "    %s", p);
		if(strchr(p, '\n') == NULL) {
			fprintf(fp, "\n");
		}
	}
	pyclose(xfp);
}

static void tb_printinternal(tb, fp)tracebackobject* tb;
									FILE* fp;
{
	while(tb != NULL) {
		fprintf(fp, "  File \"");
		py_object_print(tb->tb_frame->code->filename, fp, PY_PRINT_RAW);
		fprintf(fp, "\", line %d\n", tb->tb_lineno);
		tb_displayline(
				fp, py_string_get_value(tb->tb_frame->code->filename),
				tb->tb_lineno);
		tb = tb->tb_next;
	}
}

int py_traceback_print(v, fp)struct py_object* v;
							 FILE* fp;
{
	if(v == NULL) {
		return 0;
	}
	if(!is_tracebackobject(v)) {
		py_error_set_badcall();
		return -1;
	}

	tb_printinternal((tracebackobject*) v, fp);
	return 0;
}
