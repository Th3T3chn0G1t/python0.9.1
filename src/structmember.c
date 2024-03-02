/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Map C struct members to Python object attributes */

#include <python/structmember.h>
#include <python/errors.h>

#include <python/methodobject.h>
#include <python/intobject.h>
#include <python/floatobject.h>
#include <python/stringobject.h>

struct py_object* py_struct_get(
		void* addr, struct py_structmember* mlist, const char* name) {

	struct py_structmember* l;
	char* p = addr;

	for(l = mlist; l->name != NULL; l++) {
		if(strcmp(l->name, name) == 0) {
			struct py_object* v;
			p += l->offset;
			switch(l->type) {
				case PY_TYPE_SHORT: {
					v = py_int_new((long) *(short*) p);
					break;
				}
				case PY_TYPE_INT: {
					v = py_int_new((long) *(int*) p);
					break;
				}
				case PY_TYPE_LONG: {
					v = py_int_new(*(long*) p);
					break;
				}
				case PY_TYPE_FLOAT: {
					v = py_float_new((double) *(float*) p);
					break;
				}
				case PY_TYPE_DOUBLE: {
					v = py_float_new(*(double*) p);
					break;
				}
				case PY_TYPE_STRING: {
					if(*(char**) p == NULL) {
						py_object_incref(PY_NONE);
						v = PY_NONE;
					}
					else v = py_string_new(*(char**) p);

					break;
				}
				case PY_TYPE_OBJECT: {
					v = *(struct py_object**) p;
					if(v == NULL) v = PY_NONE;

					py_object_incref(v);
					break;
				}

				default: {
					py_error_set_string(
							py_system_error, "bad memberlist type");
					v = NULL;
				}
			}

			return v;
		}
	}

	py_error_set_string(py_name_error, name);
	return NULL;
}

int py_struct_set(addr, mlist, name, v)char* addr;
										   struct py_structmember* mlist;
										   char* name;
										   struct py_object* v;
{
	struct py_structmember* l;

	for(l = mlist; l->name != NULL; l++) {
		if(strcmp(l->name, name) == 0) {
			if(l->readonly || l->type == PY_TYPE_STRING) {
				py_error_set_string(py_runtime_error, "readonly attribute");
				return -1;
			}
			addr += l->offset;
			switch(l->type) {
				case PY_TYPE_SHORT: {
					if(!py_is_int(v)) {
						py_error_set_badarg();
						return -1;
					}

					*(short*) addr = (short) py_int_get(v);
					break;
				}

				case PY_TYPE_INT: {
					if(!py_is_int(v)) {
						py_error_set_badarg();
						return -1;
					}

					*(int*) addr = py_int_get(v);
					break;
				}

				case PY_TYPE_LONG: {
					if(!py_is_int(v)) {
						py_error_set_badarg();
						return -1;
					}

					*(long*) addr = py_int_get(v);
					break;
				}

				case PY_TYPE_FLOAT: {
					if(py_is_int(v)) {
						*(float*) addr = (float) py_int_get(v);
					}
					else if(py_is_float(v)) {
						*(float*) addr = (float) py_float_get(v);
					}
					else {
						py_error_set_badarg();
						return -1;
					}

					break;
				}

				case PY_TYPE_DOUBLE: {
					if(py_is_int(v)) *(double*) addr = py_int_get(v);
					else if(py_is_float(v)) *(double*) addr = py_float_get(v);
					else {
						py_error_set_badarg();
						return -1;
					}

					break;
				}

				case PY_TYPE_OBJECT: {
					*(struct py_object**) addr = py_object_incref(v);
					break;
				}

				default: {
					py_error_set_string(
							py_system_error, "bad memberlist type");
					return -1;
				}
			}

			return 0;
		}
	}

	py_error_set_string(py_name_error, name);
	return -1;
}
