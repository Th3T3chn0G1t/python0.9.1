/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* String object implementation */

#include <python/std.h>

#include <python/object/string.h>

struct py_object* py_string_new_size(const char* str, unsigned size) {
	struct py_string* op;

	if(!(op = malloc(sizeof(struct py_string) + size))) return 0;

	py_object_newref(op);
	op->ob.type = PY_TYPE_STRING;
	op->ob.size = size;

	memcpy(op->value, str, size);

	op->value[size] = '\0';

	return (void*) op;
}

struct py_object* py_string_new(const char* str) {
	return py_string_new_size(str, strlen(str));
}

const char* py_string_get(const struct py_object* op) {
	return ((struct py_string*) op)->value;
}

/* Methods */

struct py_object* py_string_cat(struct py_object* a, struct py_object* b) {
	struct py_string* op;

	unsigned sz_a = py_varobject_size(a);
	unsigned sz_b = py_varobject_size(b);
	unsigned size = sz_a + sz_b;

	/* Optimize cases with empty left or right operand */
	if(sz_a == 0) return py_object_incref(b);
	if(sz_b == 0) return py_object_incref(a);

	/* TODO: Not using _new_size? */
	if(!(op = malloc(sizeof(struct py_string) + size))) return 0;

	py_object_newref(op);
	op->ob.type = PY_TYPE_STRING;
	op->ob.size = size;

	memcpy(op->value, py_string_get(a), sz_a);
	memcpy(op->value + sz_a, py_string_get(b), sz_b);

	op->value[size] = '\0';

	return (void*) op;
}

/* String slice a[i:j] consists of characters a[i] ... a[j-1] */

struct py_object* py_string_slice(
		struct py_object* op, unsigned i, unsigned j) {

	if(j > py_varobject_size(op)) j = py_varobject_size(op);

	/* It's the same as op */
	if(i == 0 && j == py_varobject_size(op)) return py_object_incref(op);

	if(j < i) j = i;

	return py_string_new_size(py_string_get(op) + i, j - i);
}

struct py_object* py_string_ind(struct py_object* a, unsigned i) {
	return py_string_slice(a, i, i + 1);
}

int py_string_cmp(const struct py_object* a, const struct py_object* b) {
	unsigned sz_a = py_varobject_size(a);
	unsigned sz_b = py_varobject_size(b);
	unsigned min_len = (sz_a < sz_b) ? sz_a : sz_b;

	int cmp = memcmp(py_string_get(a), py_string_get(b), min_len);
	if(cmp != 0) return cmp;

	if(sz_a < sz_b) return -1;
	if(sz_a > sz_b) return 1;

	return 0;
}
