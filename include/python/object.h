/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Object and type object interface */

#ifndef PY_OBJECT_H
#define PY_OBJECT_H

#include <python/std.h>

/*
 * Objects are structures allocated on the heap. Special rules apply to
 * the use of objects to ensure they are properly garbage-collected.
 * Objects are never allocated statically or on the stack; they must be
 * accessed through special macros and functions only. (Type objects are
 * exceptions to the first rule; the standard types are represented by
 * statically initialized type objects.)
 *
 * An object has a 'reference count' that is increased or decreased when a
 * pointer to the object is copied or deleted; when the reference count
 * reaches zero there are no references to the object left and it can be
 * removed from the heap.
 *
 * An object has a 'type' that determines what it represents and what kind
 * of data it contains. An object's type is fixed when it is created.
 * Types themselves are represented as objects; an object contains a
 * pointer to the corresponding type object. The type itself has a type
 * pointer pointing to the object representing the type 'type', which
 * contains a pointer to itself).
 *
 * Objects do not float around in memory; once allocated an object keeps
 * the same size and address. Objects that must hold variable-size data
 * can contain pointers to variable-size parts of the object. Not all
 * objects of the same type have the same size; but the size cannot change
 * after allocation. (These restrictions are made so a reference to an
 * object can be simply a pointer -- moving an object would require
 * updating all the pointers, and changing an object's size would require
 * moving it if there was another object right next to it.)
 *
 * Objects are always accessed through pointers of the type 'struct py_object*'.
 * The type 'object' is a structure that only contains the reference count
 * and the type pointer. The actual memory allocated for an object
 * contains other data that can only be accessed after casting the pointer
 * to a pointer to a longer structure type. This longer type must start
 * with the reference count and type fields; the macro PY_OB_SEQ should be
 * used for this (to accommodate for future changes). The implementation
 * of a particular object type can cast the object pointer to the proper
 * type and back.
 *
 * A standard interface exists for objects that contain an array of items
 * whose size is determined when the object is allocated.
 *
 */

#ifndef NDEBUG
/* Turn on heavy reference debugging */
/* TODO: Fix this */
/* # define PY_REF_TRACE */
/* Turn on reference counting */
# define PY_REF_DEBUG
#endif

/* TODO: Fix unsigned/signed size mixing everywhere. */

struct py_type;

/* TODO: Object pool to avoid malloc/free constantly. */

struct py_object {
	/* TODO: Type as index into type table (8 byte objects!) */
	const struct py_type* type;

	unsigned refcount;

#ifdef PY_REF_TRACE
	struct py_object* next;
	struct py_object* prev;
#endif
};

/* TODO: This might not need to exist (and entails a gap). */
struct py_varobject {
	struct py_type* type;

	unsigned refcount;
	unsigned size;

#ifdef PY_REF_TRACE
	struct py_object* next;
	struct py_object* prev;
#endif
};

/*
 * Type objects contain a string containing the type name (to help somewhat
 * in debugging), the allocation parameters (see newobj() and newvarobj()),
 * and methods for accessing objects of the type. Methods are optional,a
 * nil pointer meaning that particular kind of access is not available for
 * this type. The py_object_decref() macro uses the dealloc method without
 * checking for a nil pointer; it should always be implemented except if
 * the implementation can guarantee that the reference count will never
 * reach zero (e.g., for type objects).
 *
 * NB: the methods for certain type groups are now contained in separate
 * method blocks.
 */

struct py_sequencemethods {
	struct py_object* (*cat)(struct py_object*, struct py_object*);
	struct py_object* (*ind)(struct py_object*, unsigned);
	struct py_object* (*slice)(struct py_object*, unsigned, unsigned);
};

enum py_print_mode {
	PY_PRINT_NORMAL,
	PY_PRINT_RAW
};

struct py_type {
	struct py_object ob;

	unsigned size; /* For allocation */

	/* Methods to implement standard operations */
	void (*dealloc)(struct py_object*);
	int (*cmp)(const struct py_object*, const struct py_object*);

	struct py_sequencemethods* sequencemethods;
};

/* TODO: Python global state. */
extern struct py_type py_type_type; /* The type of type objects */

/* Generic operations on objects */

/*
 * NEWOBJ(type, typeobj) allocates memory for a new object of the given
 * type; here 'type' must be the C structure type used to represent the
 * object and 'typeobj' the address of the corresponding type object.
 * Reference count and type pointer are filled in; the rest of the bytes of
 * the object are *undefined*!  The resulting expression type is 'type *'.
 * The size of the object is actually determined by the size field
 * of the type object.
 */

/* `void*' for convenience's sake. */
void* py_object_new(const struct py_type*);
void py_object_delete(struct py_object* p);
int py_object_cmp(const struct py_object*, const struct py_object*);
struct py_object* py_object_get_attr(struct py_object*, const char*);

unsigned py_varobject_size(const void*);

int py_is_sequence(const void*);

/*
 * py_object_incref and py_object_decref are used to increment or decrement
 * reference counts. py_object_decref calls the object's deallocator function;
 * for objects that don't contain references to other objects or heap memory
 * this can be the standard function free(). Both macros can be used
 * wherever a void expression is allowed. The argument shouldn't be a
 * NIL pointer. py_object_newref is used only to initialize reference
 * counts to 1; it is defined here for convenience.
 *
 * We assume that the reference count field can never overflow; this can
 * be proven when the size of the field is the same as the pointer size
 * but even with a 16-bit reference count field it is pretty unlikely so
 * we ignore the possibility. (If you are paranoid, make it a long.)
 *
 * Type objects should never be deallocated; the type pointer in an object
 * is not considered to be a reference to the type object, to save
 * complications in the deallocation function. (This is actually a
 * decision that's up to the implementer of each new type so if you want,
 * you can count such references to the type object.)
 *
 * *** WARNING*** py_object_decref must have a side-effect-free argument
 * since it may evaluate its argument multiple times. (The alternative
 * would be to mace it a proper function or assign it to a global temporary
 * variable first, both of which are slower; and in a multi-threaded
 * environment the global variable trick is not safe.)
 */

#ifdef PY_REF_DEBUG
/* TODO: Python global state. */
extern long py_ref_total;
#endif

void* py_object_incref(void*);
void* py_object_decref(void*);

void* py_object_newref(void*);
void py_object_unref(void*);

#ifdef PY_REF_TRACE
void py_print_refs(FILE*);
#endif

/*
 * py_none_object is an object of undefined type which can be used in contexts
 * where NULL (nil) is not suitable (since NULL often means 'error').
 *
 * Don't forget to apply py_object_incref() when returning this value!!!
 */

/* TODO: Python global state. */
extern struct py_object py_none_object; /* Don't use this directly */

#define PY_NONE (&py_none_object)

/*
 * More conventions
 * ================
 *
 * Argument Checking
 * -----------------
 *
 * Functions that take objects as arguments normally don't check for nil
 * arguments, but they do check the type of the argument, and return an
 * error if the function doesn't apply to the type.
 *
 * Failure Modes
 * -------------
 *
 * Functions may fail for a variety of reasons, including running out of
 * memory. This is communicated to the caller in two ways: an error string
 * is set (see errors.h), and the function result differs: functions that
 * normally return a pointer return NULL for failure, functions returning
 * an integer return -1 (which could be a legal return value too!), and
 * other functions return 0 for success and -1 for failure.
 * Callers should always check for errors before using the result.
 *
 * Reference Counts
 * ----------------
 *
 * It takes a while to get used to the proper usage of reference counts.
 *
 * Functions that create an object set the reference count to 1; such new
 * objects must be stored somewhere or destroyed again with py_object_decref().
 * Functions that 'store' objects such as py_tuple_set() and py_dict_insert()
 * don't increment the reference count of the object, since the most
 * frequent use is to store a fresh object. Functions that 'retrieve'
 * objects such as py_tuple_get() and py_dict_lookup() also don't increment
 * the reference count, since most frequently the object is only looked at
 * quickly. Thus, to retrieve an object and store it again, the caller
 * must call py_object_incref() explicitly.
 *
 * NOTE: functions that 'consume' a reference count like py_dict_insert() even
 * consume the reference if the object wasn't stored, to simplify error
 * handling.
 *
 * It seems attractive to make other functions that take an object as
 * argument consume a reference count; however this may quickly get
 * confusing (even the current practice is already confusing). Consider
 * it carefully, it may save lots of calls to py_object_incref() and py_object_decref() at
 * times.
 */

#endif
