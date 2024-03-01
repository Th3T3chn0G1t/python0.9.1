/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Generic object operations; and implementation of PY_NONE (py_none_object) */

#include <python/std.h>
#include <python/fgetsintr.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/stringobject.h>

#ifdef PY_REF_DEBUG
long py_ref_total;
#endif

/* Object allocation routines used by the NEWOBJ macro.
   These are used by the individual routines for object creation.
   Do not call them otherwise, they do not initialize the object! */
void* py_object_new(tp)struct py_type* tp;
{
	struct py_object* op = malloc(tp->basicsize);
	if(op == NULL) {
		return py_error_set_nomem();
	}
	PY_NEWREF(op);
	op->type = tp;
	return op;
}

int py_stop_print; /* Flag to indicate printing must be stopped */

static int prlevel;

void py_object_print(op, fp, flags)struct py_object* op;
							   FILE* fp;
							   enum py_print_mode flags;
{
	/* Hacks to make printing a long or recursive object interruptible */
	/* XXX Interrupts should leave a more permanent error */
	prlevel++;
	if(!py_stop_print && py_intrcheck()) {
		fprintf(fp, "\n[print interrupted]\n");
		py_stop_print = 1;
	}
	if(!py_stop_print) {
		if(op == NULL) {
			fprintf(fp, "<nil>");
		}
		else {
			if(op->refcount <= 0) {
				fprintf(fp, "(refcnt %d):", op->refcount);
			}
			if(op->type->print == NULL) {
				fprintf(
						fp, "<%s object at %p>", op->type->name,
						(void*) op);
			}
			else {
				(*op->type->print)(op, fp, flags);
			}
		}
	}
	prlevel--;
	if(prlevel == 0) {
		py_stop_print = 0;
	}
}

struct py_object* py_object_repr(v)struct py_object* v;
{
	struct py_object* w = NULL;
	/* Hacks to make converting a long or recursive object interruptible */
	prlevel++;
	if(!py_stop_print && py_intrcheck()) {
		py_stop_print = 1;
		py_error_set(py_interrupt_error);
	}
	if(!py_stop_print) {
		if(v == NULL) {
			w = py_string_new("<NULL>");
		}
		else if(v->type->repr == NULL) {
			char buf[100];
			sprintf(
					buf, "<%.80s object at %p>", v->type->name,
					(void*) v);
			w = py_string_new(buf);
		}
		else {
			w = (*v->type->repr)(v);
		}
		if(py_stop_print) {
			PY_XDECREF(w);
			w = NULL;
		}
	}
	prlevel--;
	if(prlevel == 0) {
		py_stop_print = 0;
	}
	return w;
}

int py_object_cmp(v, w)struct py_object* v, * w;
{
	struct py_type * tp;
	if(v == w) {
		return 0;
	}
	if(v == NULL) {
		return -1;
	}
	if(w == NULL) {
		return 1;
	}
	if((tp = v->type) != w->type) {
		return strcmp(tp->name, w->type->name);
	}
	if(tp->cmp == NULL) {
		return (v < w) ? -1 : 1;
	}
	return ((*tp->cmp)(v, w));
}

struct py_object* py_object_get_attr(v, name)struct py_object* v;
						char* name;
{
	if(v->type->get_attr == NULL) {
		py_error_set_string(py_type_error, "attribute-less object");
		return NULL;
	}
	else {
		return (*v->type->get_attr)(v, name);
	}
}

int py_object_set_attr(v, name, w)struct py_object* v;
					   char* name;
					   struct py_object* w;
{
	if(v->type->set_attr == NULL) {
		if(v->type->get_attr == NULL) {
			py_error_set_string(py_type_error, "attribute-less object");
		}
		else {
			py_error_set_string(py_type_error, "object has read-only attributes");
		}
		return -1;
	}
	else {
		return (*v->type->set_attr)(v, name, w);
	}
}


/*
py_none_object is usable as a non-NULL undefined value, used by the macro PY_NONE.
There is (and should be!) no way to create other objects of this type,
so there is exactly one (which is indestructible, by the way).
*/

static void none_print(op, fp, flags)struct py_object* op;
									 FILE* fp;
									 int flags;
{
	(void) op;
	(void) flags;

	fprintf(fp, "PY_NONE");
}

static struct py_object* none_repr(op)struct py_object* op;
{
	(void) op;

	return py_string_new("PY_NONE");
}

static struct py_type Notype = {
		PY_OB_SEQ_INIT(&py_type_type) 0, "PY_NONE", 0, 0,
		0,              /*dealloc*/ /*never called*/
		none_print,     /*print*/
		0,              /*get_attr*/
		0,              /*set_attr*/
		0,              /*cmp*/
		none_repr,      /*repr*/
		0,              /*numbermethods*/
		0,              /*sequencemethods*/
		0,              /*mappingmethods*/
};

struct py_object py_none_object = { PY_OB_SEQ_INIT(&Notype) };

void py_object_delete(struct py_object* p) { free(p); }

#ifdef PY_TRACE_REFS

static object refchain = {&refchain, &refchain};

PY_NEWREF(op)
	   struct py_object*op;
{
	   py_ref_total++;
	   op->refcount = 1;
	   op->next = refchain.next;
	   op->prev = &refchain;
	   refchain.next->prev = op;
	   refchain.next = op;
}

PY_UNREF(op)
	   struct py_object*op;
{
	   struct py_object*p;
	   int dbg;
	   if (op->refcount < 0) {
			   fprintf(stderr, "PY_UNREF negative refcnt\n");
			   abort();
	   }
	   for (p = refchain.next; p != &refchain; p = p->next) {
			   if (p == op)
				   break;
		   dbg++;
	   }
	   if (p == &refchain) { /* Not found */
			   fprintf(stderr, "PY_UNREF unknown object\n");
			   abort();
	   }
	   op->next->prev = op->prev;
	   op->prev->next = op->next;
}

PY_DELREF(op)
	   struct py_object*op;
{
	   PY_UNREF(op);
	   (*(op)->type->dealloc)(op);
}

printrefs(fp)
	   FILE *fp;
{
	   struct py_object*op;
	   fprintf(fp, "Remaining objects:\n");
	   for (op = refchain.next; op != &refchain; op = op->next) {
			   fprintf(fp, "[%d] ", op->refcount);
			   py_object_print(op, fp, PY_PRINT_NORMAL);
			   putc('\n', fp);
	   }
}

#endif
