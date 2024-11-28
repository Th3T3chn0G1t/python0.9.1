/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback implementation */

#include <python/compile.h>
#include <python/traceback.h>

#include <python/object/string.h>

#include <asys/log.h>
#include <asys/stream.h>

/* TODO: Python global state. */
static struct py_traceback* py_traceback_current = 0;

static struct py_traceback* py_traceback_new_frame(
		struct py_traceback* next, struct py_frame* frame, unsigned line) {

	struct py_traceback* traceback;

	if(!(traceback = py_object_new(PY_TYPE_TRACEBACK))) return 0;

	traceback->next = py_object_incref(next);
	traceback->frame = py_object_incref(frame);

	traceback->lineno = line;

	return traceback;
}

int py_traceback_new(struct py_frame* frame, unsigned line) {
	struct py_traceback* traceback;

	traceback = py_traceback_new_frame(py_traceback_current, frame, line);
	if(!traceback) return -1;

	py_object_decref(py_traceback_current);
	py_traceback_current = traceback;

	return 0;
}

struct py_object* py_traceback_get(void) {
	struct py_object* v;

	v = (struct py_object*) py_traceback_current; /* TODO: decref current? */
	py_traceback_current = 0;

	return v;
}

/* TODO: Debug info to not require sources for tb. */
static void py_traceback_print_line(const char* file, unsigned line) {
	static asys_fixed_buffer_t buffer = { 0 };

	enum asys_result result;

	struct asys_stream stream;
	unsigned i, j = 0;

	result = asys_stream_new(&stream, file);
	asys_log_result_path(__FILE__, "asys_stream_new", file, result);
	if(result) return;

	for(i = 1; i <= line;) {
		char c;

		result = asys_stream_read(&stream, 0, &c, 1);
		asys_log_result(__FILE__, "asys_stream_read", result);
		if(result) return;

#ifndef NDEBUG
		if(j >= ASYS_LENGTH(buffer)) {
			asys_log_result(
					__FILE__, "asys_stream_delete", asys_stream_delete(&stream));

			return;
		}
#endif

		if(c == '\n') i++;
		else if(i == line) buffer[j++] = c;
	}

	buffer[j] = 0;

	asys_log(__FILE__, "\t%s", buffer);

	asys_log_result(
			__FILE__, "asys_stream_delete", asys_stream_delete(&stream));
}

void py_traceback_print(struct py_object* object) {
	struct py_traceback* traceback = (struct py_traceback*) object;

	while(traceback) {
		const char* file = py_string_get(traceback->frame->code->filename);

		asys_log(__FILE__, "`%s:%d':", file, traceback->lineno);

		py_traceback_print_line(file, traceback->lineno);

		traceback = traceback->next;
	}
}

void py_traceback_dealloc(struct py_object* op) {
	struct py_traceback* tb = (struct py_traceback*) op;

	py_object_decref(tb->next);
	py_object_decref(tb->frame);

	free(op);
}
