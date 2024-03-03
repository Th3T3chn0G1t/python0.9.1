/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* String object implementation */

#include <python/std.h>
#include <python/stringobject.h>
#include <python/errors.h>

struct py_object* py_string_new_size(const char* str, unsigned size) {
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

const char* py_string_get(const struct py_object* op) {
	if(!py_is_string(op)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_string*) op)->value;
}

/* Methods */

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

/* String slice a[i:j] consists of characters a[i] ... a[j-1] */

static struct py_object* stringslice(
		struct py_object* op, unsigned i, unsigned j) {

	if(j > py_varobject_size(op)) {
		j = py_varobject_size(op);
	}

	/* It's the same as op */
	if(i == 0 && j == py_varobject_size(op)) {
		py_object_incref(op);
		return (struct py_object*) op;
	}

	if(j < i) j = i;

	return py_string_new_size(py_string_get(op) + i, j - i);
}

static struct py_object* stringitem(struct py_object* a, unsigned i) {
	if(i >= py_varobject_size(a)) {
		py_error_set_string(PY_INDEX_ERROR, "string index out of range");
		return NULL;
	}

	return stringslice(a, i, i + 1);
}

static int stringcompare(
		const struct py_object* a, const struct py_object* b) {

	unsigned len_a = py_varobject_size(a);
	unsigned len_b = py_varobject_size(b);
	unsigned min_len = (len_a < len_b) ? len_a : len_b;

	int cmp = memcmp(py_string_get(a), py_string_get(b), min_len);
	if(cmp != 0) return cmp;

	if(len_a < len_b) return -1;
	if(len_a > len_b) return 1;

	return 0;
}

static struct py_sequencemethods string_as_sequence = {
		stringconcat, /* tp_concat */
		stringitem, /* tp_ind */
		stringslice, /* tp_slice */
};

struct py_type py_string_type = {
		{ 1, 0, &py_type_type }, "string", sizeof(struct py_string),
		py_object_delete, /* dealloc */
		0, /* get_attr */
		0, /* set_attr */
		stringcompare, /* cmp */
		&string_as_sequence, /* sequencemethods */
};
