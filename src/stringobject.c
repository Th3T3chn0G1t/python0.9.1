/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* String object implementation */

#include <python/std.h>
#include <python/stringobject.h>
#include <python/errors.h>

struct py_object* py_string_new_size(str, size)const char* str;
											   int size;
{
	struct py_string* op = malloc(
			sizeof(struct py_string) + size * sizeof(char));
	if(op == NULL) return py_error_set_nomem();

	py_object_newref(op);
	op->ob.type = &py_string_type;
	op->ob.size = size;
	if(str != NULL) {
		memcpy(op->value, str, size);
	}
	op->value[size] = '\0';
	return (struct py_object*) op;
}

struct py_object* py_string_new(const char* str) {
	unsigned size = strlen(str);
	struct py_string* op = malloc(
			sizeof(struct py_string) + size * sizeof(char));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	py_object_newref(op);
	op->ob.type = &py_string_type;
	op->ob.size = size;
	strcpy(op->value, str);
	return (struct py_object*) op;
}

char* py_string_get_value(struct py_object* op) {
	if(!py_is_string(op)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_string*) op)->value;
}

/* Methods */

static void stringprint(op, fp, flags)struct py_string* op;
									  FILE* fp;
									  int flags;
{
	int i;
	char c;
	if(flags & PY_PRINT_RAW) {
		fwrite(op->value, 1, (int) op->ob.size, fp);
		return;
	}
	fprintf(fp, "'");
	for(i = 0; i < (int) op->ob.size; i++) {
		c = op->value[i];
		if(c == '\'' || c == '\\') {
			fprintf(fp, "\\%c", c);
		}
		else if(c < ' ' || c >= 0177) {
			fprintf(fp, "\\%03o", c & 0377);
		}
		else {
			putc(c, fp);
		}
	}
	fprintf(fp, "'");
}

static struct py_object* stringrepr(op)struct py_string* op;
{
	/* XXX overflow? */
	int newsize = 2 + 4 * op->ob.size * sizeof(char);
	struct py_object* v = py_string_new_size(NULL, newsize);
	if(v == NULL) {
		return py_error_set_nomem();
	}
	else {
		int i;
		char c;
		char* p;
		py_object_newref(v);
		v->type = &py_string_type;
		((struct py_varobject*) v)->size = newsize;
		p = ((struct py_string*) v)->value;
		*p++ = '\'';
		for(i = 0; i < (int) op->ob.size; i++) {
			c = op->value[i];
			if(c == '\'' || c == '\\') {
				*p++ = '\\', *p++ = c;
			}
			else if(c < ' ' || c >= 0177) {
				sprintf(p, "\\%03o", c & 0377);
				while(*p != '\0')
					p++;

			}
			else {
				*p++ = c;
			}
		}
		*p++ = '\'';
		*p = '\0';
		py_string_resize(&v, (int) (p - ((struct py_string*) v)->value));
		return v;
	}
}

static struct py_object* stringconcat(a, bb)struct py_string* a;
											struct py_object* bb;
{
	unsigned size;
	struct py_string* op;
	struct py_string* b;
	if(!py_is_string(bb)) {
		py_error_set_badarg();
		return NULL;
	}
	b = ((struct py_string*) bb);
	/* Optimize cases with empty left or right operand */
	if(a->ob.size == 0) {
		py_object_incref(bb);
		return bb;
	}
	if(b->ob.size == 0) {
		py_object_incref(a);
		return (struct py_object*) a;
	}
	size = a->ob.size + b->ob.size;
	op = malloc(sizeof(struct py_string) + size * sizeof(char));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	py_object_newref(op);
	op->ob.type = &py_string_type;
	op->ob.size = size;
	memcpy(op->value, a->value, (int) a->ob.size);
	memcpy(op->value + a->ob.size, b->value, (int) b->ob.size);
	op->value[size] = '\0';
	return (struct py_object*) op;
}

static struct py_object* stringrepeat(a, n)struct py_string* a;
										   int n;
{
	int i;
	unsigned size;
	struct py_string* op;
	if(n < 0) {
		n = 0;
	}
	size = a->ob.size * n;
	if(size == a->ob.size) {
		py_object_incref(a);
		return (struct py_object*) a;
	}
	op = malloc(sizeof(struct py_string) + size * sizeof(char));
	if(op == NULL) {
		return py_error_set_nomem();
	}
	py_object_newref(op);
	op->ob.type = &py_string_type;
	op->ob.size = size;
	for(i = 0; i < (int) size; i += a->ob.size) {
		memcpy(op->value + i, a->value, (int) a->ob.size);
	}
	op->value[size] = '\0';
	return (struct py_object*) op;
}

/* String slice a[i:j] consists of characters a[i] ... a[j-1] */

static struct py_object* stringslice(a, i, j)struct py_string* a;
											 int i, j; /* May be negative! */
{
	if(i < 0) {
		i = 0;
	}
	if(j < 0) {
		j = 0;
	} /* Avoid signed/unsigned bug in next line */
	if(j > (int) a->ob.size) {
		j = (int) a->ob.size;
	}
	if(i == 0 && j == (int) a->ob.size) { /* It's the same as a */
		py_object_incref(a);
		return (struct py_object*) a;
	}
	if(j < i) {
		j = i;
	}
	return py_string_new_size(a->value + i, (int) (j - i));
}

static struct py_object* stringitem(a, i)struct py_string* a;
										 int i;
{
	if(i < 0 || i >= (int) a->ob.size) {
		py_error_set_string(PY_INDEX_ERROR, "string index out of range");
		return NULL;
	}
	return stringslice(a, i, i + 1);
}

static int stringcompare(a, b)struct py_string* a, * b;
{
	int len_a = a->ob.size, len_b = b->ob.size;
	int min_len = (len_a < len_b) ? len_a : len_b;
	int cmp = memcmp(a->value, b->value, min_len);
	if(cmp != 0) {
		return cmp;
	}
	return (len_a < len_b) ? -1 : (len_a > len_b) ? 1 : 0;
}

static struct py_sequencemethods string_as_sequence = {
		stringconcat,   /*tp_concat*/
		stringrepeat,   /*tp_repeat*/
		stringitem,     /*tp_item*/
		stringslice,    /*tp_slice*/
		0,      /*tp_ass_item*/
		0,      /*tp_ass_slice*/
};

struct py_type py_string_type = {
		{ 1, &py_type_type, 0 }, "string", sizeof(struct py_string),
		sizeof(char), py_object_delete,  /*dealloc*/
		stringprint,    /*print*/
		0,              /*get_attr*/
		0,              /*set_attr*/
		stringcompare,  /*cmp*/
		stringrepr,     /*repr*/
		0,              /*numbermethods*/
		&string_as_sequence,        /*sequencemethods*/
		0,              /*mappingmethods*/
};

void py_string_join(pv, w)struct py_object** pv;
						  struct py_object* w;
{
	struct py_object* v;
	if(*pv == NULL || w == NULL || !py_is_string(*pv)) {
		return;
	}
	v = stringconcat((struct py_string*) *pv, w);
	py_object_decref(*pv);
	*pv = v;
}

/* The following function breaks the notion that strings are immutable:
   it changes the size of a string. We get away with this only if there
   is only one module referencing the object. You can also think of it
   as creating a new string object and destroying the old one, only
   more efficiently. In any case, don't use this if the string may
   already be known to some other part of the code... */
/* TODO: Remove/rework. */
int py_string_resize(pv, newsize)struct py_object** pv;
								 int newsize;
{
	struct py_object* v;
	struct py_string* sv;

	v = *pv;
	if(!py_is_string(v) || v->refcount != 1) {
		*pv = 0;
		py_object_decref(v);
		py_error_set_badcall();
		return -1;
	}

	/* XXX PY_UNREF/py_object_newref interface should be more symmetrical */
#ifdef PY_REF_DEBUG
	--py_ref_total;
#endif

	py_object_unref(v);

	*pv = realloc(v, sizeof(struct py_string) + newsize * sizeof(char));

	if(*pv == NULL) {
		/* NOTE: GCC seems to fail to see this as being okay? */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
		free(v);
#pragma GCC diagnostic pop

		py_error_set_nomem();
		return -1;
	}

	py_object_newref(*pv);
	sv = (struct py_string*) *pv;
	sv->ob.size = newsize;
	sv->value[newsize] = '\0';
	return 0;
}
