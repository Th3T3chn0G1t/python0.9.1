/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Generic object operations; and implementation of PY_NONE (py_none_object) */

#include <python/std.h>
#include <python/errors.h>

#include <asys/log.h>

/*
 * Object allocation routines used by the NEWOBJ macro.
 * These are used by the individual routines for object creation.
 * Do not call them otherwise, they do not initialize the object!
 */
void* py_object_new(enum py_type tp) {
	struct py_object* op = malloc(py_types[tp].size);
	if(op == NULL) return py_error_set_nomem();

	py_object_newref(op);
	op->type = tp;

	return op;
}

int py_object_cmp(const struct py_object* v, const struct py_object* w) {
	if(v == w) return 0;
	if(v == NULL) return -1;
	if(w == NULL) return 1;

	if(v->type != w->type) return (v < w) ? -1 : 1;
	if(py_types[v->type].cmp == NULL) return (v < w) ? -1 : 1;

	return py_types[v->type].cmp(v, w);
}

int py_is_varobject(const void* op) {
	enum py_type type = ((struct py_varobject*) op)->type;

	return type == PY_TYPE_LIST || type == PY_TYPE_TUPLE ||
			type == PY_TYPE_STRING;
}

unsigned py_varobject_size(const void* op) {
	return ((struct py_varobject*) op)->size;
}

/*
 * `py_none_object' is usable as a non-NULL undefined value, used by the macro
 * PY_NONE. There is (and should be!) no way to create other objects of this
 * type, so there is exactly one (which is indestructible, by the way).
 */

struct py_object py_none_object = { PY_TYPE_NONE, 1 };

#ifdef PY_REF_TRACE
/* TODO: Python global state. */
static object py_refchain = { &py_refchain, &py_refchain };
#endif

#ifdef PY_REF_DEBUG
long py_ref_total = 0;
long py_object_total = 0;
#endif

void* py_object_incref(void* p) {
	struct py_object* op = p;

	if(!p) return 0;

#ifdef PY_REF_DEBUG
	py_ref_total++;
#endif

	op->refcount++;

#ifndef NDEBUG
	/* TODO: Formalise this. */
	if(op != PY_NONE && op->refcount > 10000) {
		asys_log(
				__FILE__, "Suspicious refcount `%u' on object `%p'",
				op->refcount, p);
	}
#endif

	return op;
}

void* py_object_decref(void* p) {
	struct py_object* op = p;

	if(!p) return 0;

#ifndef NDEBUG
	if(!op->refcount) {
		asys_log(__FILE__, "Possible double free on object `%p'", p);
		return 0;
	}
#endif

#ifdef PY_REF_DEBUG
	py_ref_total--;
#endif

	if(--op->refcount <= 0) {
		py_object_total--;

		py_object_unref(op);
		py_types[op->type].dealloc(op);
	}

	return op;
}

void* py_object_newref(void* p) {
	struct py_object* op = p;

	if(!p) return 0;

#ifdef PY_REF_DEBUG
	py_ref_total++;
	py_object_total++;
#endif

#ifdef PY_REF_TRACE
	op->next = py_refchain.next;
	op->prev = &py_refchain;
	py_refchain.next->prev = op;
	py_refchain.next = op;
#endif

	op->refcount = 1;

	return op;
}

void py_object_unref(void* p) {
#ifdef PY_REF_TRACE
	struct py_object* op;

	if(!p) return;

	if(op->refcount < 0) {
		fprintf(stderr, "unref negative refcnt\n");
		abort();
	}

	for(op = py_refchain.next; op != &py_refchain; op = op->next) {
		if(p == op) break;
	}

	if(op == &py_refchain) { /* Not found */
		fprintf(stderr, "unref unknown object\n");
		abort();
	}

	op->next->prev = op->prev;
	op->prev->next = op->next;
#else
	(void) p;
#endif
}

#ifdef PY_REF_TRACE
void py_print_refs(FILE* fp) {
	struct py_object*op;
	fprintf(fp, "Remaining objects:\n");
	for (op = py_refchain.next; op != &py_refchain; op = op->next) {
		fprintf(fp, "[%d] ", op->refcount);
		putc('\n', fp);
	}
}
#endif
