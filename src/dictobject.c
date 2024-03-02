/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Dictionary object implementation; using a hash table */

/*
XXX Note -- although this may look professional, I didn't think very hard
about the problem and it is possible that obvious improvements exist.
A similar module that I saw by Chris Torek:
- uses chaining instead of hashed linear probing
- remembers the hash value with the entry to speed up table resizing
- sets the table size to a power of 2
- uses a different hash function:
       h = 0; p = str; while (*p) h = (h<<5) - h + *p++;
*/

#include <python/std.h>
#include <python/modsupport.h>
#include <python/errors.h>

#include <python/stringobject.h>
#include <python/dictobject.h>
#include <python/listobject.h>
#include <python/intobject.h>

/*
Table of primes suitable as keys, in ascending order.
The first line are the largest primes less than some powers of two,
the second line is the largest prime less than 6000,
and the third line is a selection from Knuth, Vol. 3, Sec. 6.1, Table 1.
The final value is a sentinel and should cause the memory allocation
of that many entries to fail (if none of the earlier values cause such
failure already).
*/
static unsigned int primes[] = {
		3, 7, 13, 31, 61, 127, 251, 509, 1021, 2017, 4093, 5987, 9551, 15683,
		19609, 31397, 0xffffffff /* All bits set -- truncation OK */
};

/* String used as dummy key to fill deleted entries */
static struct py_string* dummy; /* Initialized by first call to py_dict_new() */

/*
Invariant for entries: when in use, de_value is not NULL and de_key is
not NULL and not dummy; when not in use, de_value is NULL and de_key
is either NULL or dummy. A dummy key value cannot be replaced by NULL,
since otherwise other keys may be lost.
*/
typedef struct {
	struct py_string* de_key;
	struct py_object* de_value;
} dictentry;

/*
To ensure the lookup algorithm terminates, the table size must be a
prime number and there must be at least one NULL key in the table.
The value di_fill is the number of non-NULL keys; di_used is the number
of non-NULL, non-dummy keys.
To avoid slowing down lookups on a near-full table, we resize the table
when it is more than half filled.
*/
typedef struct {
	PY_OB_SEQ
	int di_fill;
	int di_used;
	int di_size;
	dictentry* di_table;
} dictobject;

struct py_object* py_dict_new() {
	dictobject* dp;
	if(dummy == NULL) { /* Auto-initialize dummy */
		dummy = (struct py_string*) py_string_new("");
		if(dummy == NULL) {
			return NULL;
		}
	}
	dp = py_object_new(&py_dict_type);
	if(dp == NULL) {
		return NULL;
	}
	dp->di_size = primes[0];
	dp->di_table = (dictentry*) calloc(sizeof(dictentry), dp->di_size);
	if(dp->di_table == NULL) {
		free(dp);
		return py_error_set_nomem();
	}
	dp->di_fill = 0;
	dp->di_used = 0;
	return (struct py_object*) dp;
}

/*
The basic lookup function used by all operations.
This is essentially Algorithm D from Knuth Vol. 3, Sec. 6.4.
Open addressing is preferred over chaining since the link overhead for
chaining would be substantial (100% with typical malloc overhead).

First a 32-bit hash value, 'sum', is computed from the key string.
The first character is added an extra time shifted by 8 to avoid hashing
single-character keys (often heavily used variables) too close together.
All arithmetic on sum should ignore overflow.

The initial probe index is then computed as sum mod the table size.
Subsequent probe indices are incr apart (mod table size), where incr
is also derived from sum, with the additional requirement that it is
relative prime to the table size (i.e., 1 <= incr < size, since the size
is a prime number). My choice for incr is somewhat arbitrary.
*/

static dictentry* lookdict(dp, key)dictobject* dp;
								   const char* key;
{
	int i, incr;
	dictentry* freeslot = NULL;
	unsigned char* p = (unsigned char*) key;
	unsigned long sum = *p << 7;
	while(*p != '\0')
		sum = sum + sum + *p++;
	i = sum % dp->di_size;
	do {
		sum = sum + sum + 1;
		incr = sum % dp->di_size;
	} while(incr == 0);
	for(;;) {
		dictentry* ep = &dp->di_table[i];
		if(ep->de_key == NULL) {
			if(freeslot != NULL) {
				return freeslot;
			}
			else {
				return ep;
			}
		}
		if(ep->de_key == dummy) {
			if(freeslot != NULL) {
				freeslot = ep;
			}
		}
		else if(GETSTRINGVALUE(ep->de_key)[0] == key[0]) {
			if(strcmp(GETSTRINGVALUE(ep->de_key), key) == 0) {
				return ep;
			}
		}
		i = (i + incr) % dp->di_size;
	}
}

/*
Internal routine to insert a new item into the table.
Used both by the internal resize routine and by the public insert routine.
Eats a reference to key and one to value.
*/
static void
insertdict(dictobject* dp, struct py_string* key, struct py_object* value) {
	dictentry* ep;
	ep = lookdict(dp, GETSTRINGVALUE(key));
	if(ep->de_value != NULL) {
		PY_DECREF(ep->de_value);
		PY_DECREF(key);
	}
	else {
		if(ep->de_key == NULL) {
			dp->di_fill++;
		}
		else PY_DECREF(ep->de_key);
		ep->de_key = key;
		dp->di_used++;
	}
	ep->de_value = value;
}

/*
Restructure the table by allocating a new table and reinserting all
items again. When entries have been deleted, the new table may
actually be smaller than the old one.
*/

static int dictresize(dictobject* dp) {
	int oldsize = dp->di_size;
	int newsize;
	dictentry* oldtable = dp->di_table;
	dictentry* newtable;
	dictentry* ep;
	int i;
	newsize = dp->di_size;
	for(i = 0;; i++) {
		if(primes[i] > (unsigned) (dp->di_used * 2)) {
			newsize = primes[i];
			break;
		}
	}
	newtable = (dictentry*) calloc(sizeof(dictentry), newsize);
	if(newtable == NULL) {
		py_error_set_nomem();
		return -1;
	}
	dp->di_size = newsize;
	dp->di_table = newtable;
	dp->di_fill = 0;
	dp->di_used = 0;
	for(i = 0, ep = oldtable; i < oldsize; i++, ep++) {
		if(ep->de_value != NULL) {
			insertdict(dp, ep->de_key, ep->de_value);
		}
		else if(ep->de_key != NULL)
			PY_DECREF(ep->de_key);
	}
	free(oldtable);
	return 0;
}

struct py_object* py_dict_lookup(struct py_object* op, const char* key) {
	if(!py_is_dict(op)) {
		py_fatal("py_dict_lookup on non-dictionary");
	}
	return lookdict((dictobject*) op, key)->de_value;
}

#ifdef NOT_USED
static struct py_object*
dict2lookup(op, key)
	   struct py_object*op;
	   struct py_object*key;
{
	   struct py_object*res;
	   if (!py_is_dict(op)) {
			   py_error_set_badcall();
			   return NULL;
	   }
	   if (!py_is_string(key)) {
			   py_error_set_badarg();
			   return NULL;
	   }
	   res = lookdict((dictobject *)op, ((struct py_string *)key)->value)
															   -> de_value;
	   if (res == NULL)
			   py_error_set_string(PY_KEY_ERROR, "key not in dictionary");
	   return res;
}
#endif

static int dict2insert(op, key, value)struct py_object* op;
									  struct py_object* key;
									  struct py_object* value;
{
	dictobject* dp;
	struct py_string* keyobj;
	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return -1;
	}
	dp = (dictobject*) op;
	if(!py_is_string(key)) {
		py_error_set_badarg();
		return -1;
	}
	keyobj = (struct py_string*) key;
	/* if fill >= 2/3 size, resize */
	if(dp->di_fill * 3 >= dp->di_size * 2) {
		if(dictresize(dp) != 0) {
			if(dp->di_fill + 1 > dp->di_size) {
				return -1;
			}
		}
	}
	PY_INCREF(keyobj);
	PY_INCREF(value);
	insertdict(dp, keyobj, value);
	return 0;
}

int py_dict_insert(op, key, value)struct py_object* op;
								  const char* key;
								  struct py_object* value;
{
	struct py_object* keyobj;
	int err;
	keyobj = py_string_new(key);
	if(keyobj == NULL) {
		py_error_set_nomem();
		return -1;
	}
	err = dict2insert(op, keyobj, value);
	PY_DECREF(keyobj);
	return err;
}

int py_dict_remove(op, key)struct py_object* op;
						   const char* key;
{
	dictobject* dp;
	dictentry* ep;
	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return -1;
	}
	dp = (dictobject*) op;
	ep = lookdict(dp, key);
	if(ep->de_value == NULL) {
		py_error_set_string(PY_KEY_ERROR, "key not in dictionary");
		return -1;
	}
	PY_DECREF(ep->de_key);
	PY_INCREF(dummy);
	ep->de_key = dummy;
	PY_DECREF(ep->de_value);
	ep->de_value = NULL;
	dp->di_used--;
	return 0;
}

static int dict2remove(op, key)struct py_object* op;
							   struct py_object* key;
{
	if(!py_is_string(key)) {
		py_error_set_badarg();
		return -1;
	}
	return py_dict_remove(op, GETSTRINGVALUE((struct py_string*) key));
}

int py_dict_size(op)struct py_object* op;
{
	if(!py_is_dict(op)) {
		py_error_set_badcall();
		return -1;
	}
	return ((dictobject*) op)->di_size;
}

static struct py_object* getdict2key(op, i)struct py_object* op;
										   int i;
{
	/* XXX This can't return errors since its callers assume
	   that NULL means there was no key at that point */
	dictobject* dp;
	if(!py_is_dict(op)) {
		/* py_error_set_badcall(); */
		return NULL;
	}
	dp = (dictobject*) op;
	if(i < 0 || i >= dp->di_size) {
		/* py_error_set_badarg(); */
		return NULL;
	}
	if(dp->di_table[i].de_value == NULL) {
		/* Not an error! */
		return NULL;
	}
	return (struct py_object*) dp->di_table[i].de_key;
}

char* py_dict_get_key(op, i)struct py_object* op;
							int i;
{
	struct py_object* keyobj = getdict2key(op, i);
	if(keyobj == NULL) {
		return NULL;
	}
	return GETSTRINGVALUE((struct py_string*) keyobj);
}

/* Methods */

static void dict_dealloc(dp)dictobject* dp;
{
	int i;
	dictentry* ep;
	for(i = 0, ep = dp->di_table; i < dp->di_size; i++, ep++) {
		if(ep->de_key != NULL)
			PY_DECREF(ep->de_key);
		if(ep->de_value != NULL)
			PY_DECREF(ep->de_value);
	}
	if(dp->di_table != NULL) {
		free(dp->di_table);
	}
	free(dp);
}

static void dict_print(dp, fp, flags)dictobject* dp;
									 FILE* fp;
									 int flags;
{
	int i;
	int any;
	dictentry* ep;
	fprintf(fp, "{");
	any = 0;
	for(i = 0, ep = dp->di_table; i < dp->di_size; i++, ep++) {
		if(ep->de_value != NULL) {
			if(any++ > 0) {
				fprintf(fp, "; ");
			}
			py_object_print((struct py_object*) ep->de_key, fp, flags);
			fprintf(fp, ": ");
			py_object_print(ep->de_value, fp, flags);
		}
	}
	fprintf(fp, "}");
}

static void js(pv, w)struct py_object** pv;
					 struct py_object* w;
{
	py_string_join(pv, w);
	if(w != NULL)
		PY_DECREF(w);
}

static struct py_object* dict_repr(dp)dictobject* dp;
{
	auto struct py_object* v;
	struct py_object* w;
	struct py_object* semi, * colon;
	int i;
	int any;
	dictentry* ep;
	v = py_string_new("{");
	semi = py_string_new("; ");
	colon = py_string_new(": ");
	any = 0;
	for(i = 0, ep = dp->di_table; i < dp->di_size; i++, ep++) {
		if(ep->de_value != NULL) {
			if(any++) {
				py_string_join(&v, semi);
			}
			js(&v, w = py_object_repr((struct py_object*) ep->de_key));
			py_string_join(&v, colon);
			js(&v, w = py_object_repr(ep->de_value));
		}
	}
	js(&v, w = py_string_new("}"));
	if(semi != NULL)
		PY_DECREF(semi);
	if(colon != NULL)
		PY_DECREF(colon);
	return v;
}

static int dict_length(dp)dictobject* dp;
{
	return dp->di_used;
}

static struct py_object* dict_subscript(dp, v)dictobject* dp;
											  struct py_object* v;
{
	if(!py_is_string(v)) {
		py_error_set_badarg();
		return NULL;
	}
	v = lookdict(dp, GETSTRINGVALUE((struct py_string*) v))->de_value;
	if(v == NULL) {
		py_error_set_string(PY_KEY_ERROR, "key not in dictionary");
	}
	else
		PY_INCREF(v);
	return v;
}

static int dict_ass_sub(dp, v, w)dictobject* dp;
								 struct py_object* v, * w;
{
	if(w == NULL) {
		return dict2remove((struct py_object*) dp, v);
	}
	else {
		return dict2insert((struct py_object*) dp, v, w);
	}
}

static struct py_mappingmethods dict_as_mapping = {
		dict_length,    /*len*/
		dict_subscript, /*ind*/
		dict_ass_sub,   /*assign*/
};

static struct py_object* dict_keys(dp, args)dictobject* dp;
											struct py_object* args;
{
	struct py_object* v;
	int i, j;
	if(!py_arg_none(args)) {
		return NULL;
	}
	v = py_list_new(dp->di_used);
	if(v == NULL) {
		return NULL;
	}
	for(i = 0, j = 0; i < dp->di_size; i++) {
		if(dp->di_table[i].de_value != NULL) {
			struct py_string* key = dp->di_table[i].de_key;
			PY_INCREF(key);
			py_list_set(v, j, (struct py_object*) key);
			j++;
		}
	}
	return v;
}

struct py_object* py_dict_get_keys(dp)struct py_object* dp;
{
	if(dp == NULL || !py_is_dict(dp)) {
		py_error_set_badcall();
		return NULL;
	}
	return dict_keys((dictobject*) dp, (struct py_object*) NULL);
}

static struct py_object* dict_has_key(dp, args)dictobject* dp;
											   struct py_object* args;
{
	struct py_object* key;
	long ok;
	if(!py_arg_str(args, &key)) {
		return NULL;
	}
	ok = lookdict(dp, GETSTRINGVALUE((struct py_string*) key))->de_value !=
		 NULL;
	return py_int_new(ok);
}

static struct py_methodlist dict_methods[] = {
		{ "keys",    dict_keys },
		{ "has_key", dict_has_key },
		{ NULL, NULL }           /* sentinel */
};

static struct py_object* dict_getattr(dp, name)dictobject* dp;
											   char* name;
{
	return py_methodlist_find(dict_methods, (struct py_object*) dp, name);
}

void py_done_dict(void) {
	PY_DECREF(dummy);
}

struct py_type py_dict_type = {
		PY_OB_SEQ_INIT(&py_type_type) 0, "dictionary", sizeof(dictobject), 0,
		dict_dealloc,   /*dealloc*/
		dict_print,     /*print*/
		dict_getattr,   /*get_attr*/
		0,              /*set_attr*/
		0,              /*cmp*/
		dict_repr,      /*repr*/
		0,              /*numbermethods*/
		0,              /*sequencemethods*/
		&dict_as_mapping,   /*mappingmethods*/
};
