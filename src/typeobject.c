/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Type object implementation */

#include <python/std.h>
#include <python/object.h>

#include <python/stringobject.h>

struct py_type py_type_type = {
		{ 1, &py_type_type, 0 },
		"type", /* Name of this type */
		sizeof(struct py_type), /* Basic object size */
		0, /* dealloc */
		0, /* get_attr */
		0, /* set_attr */
		0, /* cmp*/
		0, 0 };
