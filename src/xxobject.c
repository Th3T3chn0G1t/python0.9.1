/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Use this file as a template to start implementing a new object type.
   If your objects will be called foobar, start by copying this file to
   foobarobject.c, changing all occurrences of xx to foobar and all
   occurrences of Xx by Foobar. You will probably want to delete all
   references to 'x_attr' and add your own types of attributes
   instead. Maybe you want to name your local variables other than
   'xp'. If your object type is needed in other files, you'll have to
   create a file "foobarobject.h"; see struct py_int.h for an example. */


/* Xx objects */

#include <python/allobjects.h>

typedef struct {
	PY_OB_SEQ
	struct py_object* x_attr;        /* Attributes dictionary */
} xxobject;

struct py_type Xxtype;      /* Really static, forward */

#define is_xxobject(v)         ((v)->type == &Xxtype)

static xxobject* newxxobject(arg)struct py_object* arg;
{
	xxobject* xp;
	xp = py_object_new(&Xxtype);
	if(xp == NULL) {
		return NULL;
	}
	xp->x_attr = NULL;
	return xp;
}

/* Xx methods */

static void xx_dealloc(xp)xxobject* xp;
{
	PY_XDECREF(xp->x_attr);
	free(xp);
}

static struct py_object* xx_demo(self, args)xxobject* self;
											struct py_object* args;
{
	if(!py_arg_none(args)) {
		return NULL;
	}
	PY_INCREF(PY_NONE);
	return PY_NONE;
}

static struct py_methodlist xx_methods[] = {
		"demo", xx_demo, { NULL, NULL }           /* sentinel */
};

static struct py_object* xx_getattr(xp, name)xxobject* xp;
											 char* name;
{
	if(xp->x_attr != NULL) {
		struct py_object* v = py_dict_lookup(xp->x_attr, name);
		if(v != NULL) {
			PY_INCREF(v);
			return v;
		}
	}
	return py_methodlist_find(xx_methods, (struct py_object*) xp, name);
}

static int xx_setattr(xp, name, v)xxobject* xp;
								  char* name;
								  struct py_object* v;
{
	if(xp->x_attr == NULL) {
		xp->x_attr = py_dict_new();
		if(xp->x_attr == NULL) {
			return -1;
		}
	}
	if(v == NULL) {
		return py_dict_remove(xp->x_attr, name);
	}
	else {
		return py_dict_insert(xp->x_attr, name, v);
	}
}

struct py_type Xxtype = {
		PY_OB_SEQ_INIT(&py_type_type) 0,                      /*size*/
		"xx",                 /*name*/
		sizeof(xxobject),       /*tp_size*/
		0,                      /*itemsize*/
		/* methods */
		xx_dealloc,     /*dealloc*/
		0,              /*print*/
		xx_getattr,     /*get_attr*/
		xx_setattr,     /*set_attr*/
		0,              /*cmp*/
		0,              /*repr*/
};
