/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Dictionary object implementation; using a hash table */

/*
 * XXX Note -- although this may look professional, I didn't think very hard
 * about the problem and it is possible that obvious improvements exist.
 * A similar module that I saw by Chris Torek:
 * - uses chaining instead of hashed linear probing
 * - remembers the hash value with the entry to speed up table resizing
 * - sets the table size to a power of 2
 * - uses a different hash function:
 *        h = 0; p = str; while (*p) h = (h<<5) - h + *p++;
 */

/* TODO: Fix overly fatal EH in here. */
/* TODO: Do dict keys need to be string *objects*? */

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/stringobject.h>
#include <python/dictobject.h>

/*
 * Table of primes suitable as keys, in ascending order.
 * The first line are the largest primes less than some powers of two,
 * the second line is the largest prime less than 6000,
 * and the third line is a selection from Knuth, Vol. 3, Sec. 6.1, Table 1.
 * The final value is a sentinel and should cause the memory allocation
 * of that many entries to fail (if none of the earlier values cause such
 * failure already).
 */

static unsigned primes[] = {
		3, 7, 13, 31, 61, 127, 251, 509, 1021, 2017, 4093, 5987, 9551, 15683,
		19609, 31397, 0xffffffff /* All bits set -- truncation OK */
};

/* String used as dummy key to fill deleted entries */
/* Initialized by first call to py_dict_new() */
/* TODO: Python global state. */
static struct py_object* dummy;

/*
 * To ensure the lookup algorithm terminates, the table size must be a
 * prime number and there must be at least one NULL key in the table.
 * The value fill is the number of non-NULL keys; used is the number
 * of non-NULL, non-dummy keys.
 * To avoid slowing down lookups on a near-full table, we resize the table
 * when it is more than half filled.
 */

int py_is_dict(const void* op) {
	return ((struct py_object*) op)->type == &py_dict_type;
}

struct py_object* py_dict_new(void) {
	struct py_dict* dp;

	/* TODO: This really doesn't need to be here. */
	if(dummy == NULL) { /* Auto-initialize dummy */
		dummy = py_string_new("");
		if(dummy == NULL) return NULL;
	}

	dp = py_object_new(&py_dict_type);
	if(dp == NULL) return NULL;

	dp->size = primes[0];

	dp->table = calloc(sizeof(struct py_dictentry), dp->size);
	if(dp->table == NULL) {
		free(dp);
		return py_error_set_nomem();
	}

	dp->fill = 0;
	dp->used = 0;

	return (struct py_object*) dp;
}

/*
 * The basic lookup function used by all operations.
 * This is essentially Algorithm D from Knuth Vol. 3, Sec. 6.4.
 * Open addressing is preferred over chaining since the link overhead for
 * chaining would be substantial (100% with typical malloc overhead).
 *
 * First a 32-bit hash value, 'sum', is computed from the key string.
 * The first character is added an extra time shifted by 8 to avoid hashing
 * single-character keys (often heavily used variables) too close together.
 * All arithmetic on sum should ignore overflow.
 *
 * The initial probe index is then computed as sum mod the table size.
 * Subsequent probe indices are incr apart (mod table size), where incr
 * is also derived from sum, with the additional requirement that it is
 * relative prime to the table size (i.e., 1 <= incr < size, since the size
 * is a prime number). My choice for incr is somewhat arbitrary.
 */

static struct py_dictentry* py_dict_look(struct py_dict* dp, const char* key) {
	unsigned i, incr;
	unsigned char* p = (unsigned char*) key;
	unsigned long sum = *p << 7;

	while(*p != '\0') sum = sum + sum + *p++;

	i = sum % dp->size;
	do {
		sum = sum + sum + 1;
		incr = sum % dp->size;
	} while(incr == 0);

	for(;;) {
		struct py_dictentry* ep = &dp->table[i];

		if(ep->key == NULL) return ep;
		else if(py_string_get(ep->key)[0] == key[0]) {
			if(strcmp(py_string_get(ep->key), key) == 0) return ep;
		}

		i = (i + incr) % dp->size;
	}
}

/*
 * Internal routine to insert a new item into the table.
 * Used both by the internal resize routine and by the public insert routine.
 * Eats a reference to key and one to value.
 */
static void py_dict_table_insert(
		struct py_dict* dp, struct py_object* key, struct py_object* value) {

	struct py_dictentry* ep;

	ep = py_dict_look(dp, py_string_get(key));

	if(ep->value != NULL) {
		py_object_decref(ep->value);
		py_object_decref(key);
	}
	else {
		if(ep->key == NULL) dp->fill++;
		else py_object_decref(ep->key);

		ep->key = key;
		dp->used++;
	}

	ep->value = value;
}

/*
 * Restructure the table by allocating a new table and reinserting all
 * items again. When entries have been deleted, the new table may
 * actually be smaller than the old one.
 */
static int py_dict_resize(struct py_dict* dp) {
	unsigned oldsize = dp->size;
	unsigned newsize;
	struct py_dictentry* oldtable = dp->table;
	struct py_dictentry* newtable;
	struct py_dictentry* ep;
	unsigned i;

	for(i = 0;; i++) {
		if(primes[i] > (unsigned) (dp->used * 2)) {
			newsize = primes[i];
			break;
		}
	}

	newtable = calloc(sizeof(struct py_dictentry), newsize);
	if(newtable == NULL) {
		py_error_set_nomem();
		return -1;
	}

	dp->size = newsize;
	dp->table = newtable;
	dp->fill = 0;
	dp->used = 0;

	for(i = 0, ep = oldtable; i < oldsize; i++, ep++) {
		if(ep->value != NULL) py_dict_table_insert(dp, ep->key, ep->value);
		else if(ep->key != NULL) py_object_decref(ep->key);
	}

	free(oldtable);
	return 0;
}

struct py_object* py_dict_lookup(struct py_object* op, const char* key) {
	if(!py_is_dict(op)) py_fatal("py_dict_lookup on non-dictionary");

	return py_dict_look((struct py_dict*) op, key)->value;
}

static int py_dict_insert_impl(
		struct py_object* op, struct py_object* key, struct py_object* value) {

	struct py_dict* dp;
	struct py_object* keyobj;

	/* TODO: Non-typechecked builds. */
	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return -1;
	}

	dp = (struct py_dict*) op;
	if(!py_is_string(key)) {
		py_error_set_badarg();
		return -1;
	}

	keyobj = key;

	/* if fill >= 2/3 size, resize */
	if(dp->fill * 3 >= dp->size * 2) {
		if(py_dict_resize(dp) != 0) {
			if(dp->fill + 1 > dp->size) return -1;
		}
	}

	py_object_incref(keyobj);
	py_object_incref(value);
	py_dict_table_insert(dp, keyobj, value);

	return 0;
}

int py_dict_insert(
		struct py_object* op, const char* key, struct py_object* value) {

	struct py_object* keyobj;
	int err;

	keyobj = py_string_new(key);
	if(keyobj == NULL) {
		py_error_set_nomem();
		return -1;
	}

	if(!py_is_string(keyobj)) puts("???");

	err = py_dict_insert_impl(op, keyobj, value);
	py_object_decref(keyobj);

	return err;
}

int py_dict_remove(struct py_object* op, const char* key) {
	struct py_dict* dp;
	struct py_dictentry* ep;

	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return -1;
	}

	dp = (struct py_dict*) op;
	ep = py_dict_look(dp, key);

	if(ep->value == NULL) {
		py_error_set_string(PY_KEY_ERROR, "key not in dictionary");
		return -1;
	}

	py_object_decref(ep->key);
	py_object_incref(dummy);

	ep->key = dummy;
	py_object_decref(ep->value);
	ep->value = NULL;
	dp->used--;

	return 0;
}

static int py_dict_remove_impl(struct py_object* op, struct py_object* key) {
	if(!py_is_string(key)) {
		py_error_set_badarg();
		return -1;
	}

	return py_dict_remove(op, py_string_get(key));
}

/* TODO: Dicts as varobjects? */
unsigned py_dict_size(struct py_object* op) {
	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return UINT_MAX; /* TODO: No one seems to check this? */
	}

	return ((struct py_dict*) op)->size;
}

static struct py_object* py_dict_get_key_impl(
		struct py_object* op, unsigned i) {

	/*
	 * XXX This can't return errors since its callers assume
	 * that NULL means there was no key at that point
	 */
	/* TODO: Better EH. */

	struct py_dict* dp;

	if(!py_is_dict(op)) {
		/* py_error_set_badcall(); */
		return NULL;
	}

	dp = (struct py_dict*) op;

	if(i >= dp->size) {
		/* py_error_set_badarg(); */
		return NULL;
	}

	if(dp->table[i].value == NULL) {
		/* Not an error! */
		return NULL;
	}

	return (struct py_object*) dp->table[i].key;
}

const char* py_dict_get_key(struct py_object* op, unsigned i) {
	struct py_object* keyobj = py_dict_get_key_impl(op, i);
	if(keyobj == NULL) return NULL;

	return py_string_get(keyobj);
}

/* Methods */

static void dict_dealloc(struct py_object* op) {
	struct py_dict* dp = (struct py_dict*) op;
	struct py_dictentry* ep;
	unsigned i;

	for(i = 0, ep = dp->table; i < dp->size; i++, ep++) {
		if(ep->key != NULL) py_object_decref(ep->key);
		if(ep->value != NULL) py_object_decref(ep->value);
	}

	if(dp->table != NULL) free(dp->table);
}

struct py_object* py_dict_lookup_object(
		struct py_object* dp, struct py_object* v) {

	if(!py_is_string(v)) {
		py_error_set_badarg();
		return NULL;
	}

	v = py_dict_look((struct py_dict*) dp, py_string_get(v))->value;

	if(v == NULL) py_error_set_string(PY_KEY_ERROR, "key not in dictionary");
	else py_object_incref(v);

	return v;
}

int py_dict_assign(
		struct py_object* dp, struct py_object* v, struct py_object* w) {

	if(w == NULL) return py_dict_remove_impl((struct py_object*) dp, v);
	else return py_dict_insert_impl((struct py_object*) dp, v, w);
}

void py_done_dict(void) {
	py_object_decref(dummy);
}

struct py_type py_dict_type = {
		{ &py_type_type, 1 }, sizeof(struct py_dict),
		dict_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};
