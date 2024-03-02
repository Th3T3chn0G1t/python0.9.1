/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Type object implementation */

#include <python/std.h>
#include <python/object.h>

#include <python/stringobject.h>

static void type_print(v, fp, flags)struct py_type* v;
									FILE* fp;
									int flags;
{
	(void) flags;

	fprintf(fp, "<type '%s'>", v->name);
}

struct py_type py_type_type = {
		{ 1, &py_type_type, 0 },
		"type", /* Name of this type */
		sizeof(struct py_type), /* Basic object size */
		0, /* dealloc */
		type_print, /* print */
		0, /* get_attr */
		0, /* set_attr */
		0, /* cmp*/
		0, 0, 0 };
