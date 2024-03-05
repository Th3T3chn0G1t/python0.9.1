/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Instruction opcodes for compiled code */

#ifndef PY_OPCODE_H
#define PY_OPCODE_H

enum py_opcode {
	PY_OP_POP_TOP = 1,
	PY_OP_ROT_TWO = 2,
	PY_OP_ROT_THREE = 3,
	PY_OP_DUP_TOP = 4,

	PY_OP_UNARY_NEGATIVE = 11,
	PY_OP_UNARY_NOT = 12,
	PY_OP_UNARY_CALL = 14,

	PY_OP_BINARY_MULTIPLY = 20,
	PY_OP_BINARY_DIVIDE = 21,
	PY_OP_BINARY_MODULO = 22,
	PY_OP_BINARY_ADD = 23,
	PY_OP_BINARY_SUBTRACT = 24,
	PY_OP_BINARY_SUBSCR = 25,
	PY_OP_BINARY_CALL = 26,

	PY_OP_SLICE = 30,
	/* Also uses 31-33 */

	PY_OP_STORE_SUBSCR = 60,

	PY_OP_PRINT_EXPR = 70,

	PY_OP_BREAK_LOOP = 80,
	PY_OP_LOAD_LOCALS = 82,
	PY_OP_RETURN_VALUE = 83,
	PY_OP_REQUIRE_ARGS = 84,
	PY_OP_REFUSE_ARGS = 85,
	PY_OP_BUILD_FUNCTION = 86,
	PY_OP_POP_BLOCK = 87,
	PY_OP_BUILD_CLASS = 89,

	PY_OP_HAVE_ARGUMENT = 90, /* Opcodes from here have an argument: */

	PY_OP_STORE_NAME = 90, /* Index in name list */
	PY_OP_UNPACK_TUPLE = 92, /* Number of tuple items */
	PY_OP_UNPACK_LIST = 93, /* Number of list items */

	PY_OP_STORE_ATTR = 95, /* Index in name list */

	PY_OP_LOAD_CONST = 100, /* Index in const list */
	PY_OP_LOAD_NAME = 101, /* Index in name list */
	PY_OP_BUILD_TUPLE = 102, /* Number of tuple items */
	PY_OP_BUILD_LIST = 103, /* Number of list items */
	PY_OP_BUILD_MAP = 104, /* Always zero for now */
	PY_OP_LOAD_ATTR = 105, /* Index in name list */
	PY_OP_COMPARE_OP = 106, /* Comparison operator */
	PY_OP_IMPORT_NAME = 107, /* Index in name list */
	PY_OP_IMPORT_FROM = 108, /* Index in name list */

	PY_OP_JUMP_FORWARD = 110, /* Number of bytes to skip */
	PY_OP_JUMP_IF_FALSE = 111, /* "" */
	PY_OP_JUMP_IF_TRUE = 112, /* "" */
	PY_OP_JUMP_ABSOLUTE = 113, /* Target byte offset from beginning of code */
	PY_OP_FOR_LOOP = 114, /* Number of bytes to skip */

	PY_OP_SETUP_LOOP = 120, /* Target address (absolute) */
	PY_OP_SETUP_EXCEPT = 121, /* "" */

	PY_OP_SET_LINENO = 127 /* Current line number */
};

/* Comparison operator codes (argument to PY_OP_COMPARE_OP) */
enum py_cmp_op {
	PY_CMP_LT,
	PY_CMP_LE,
	PY_CMP_EQ,
	PY_CMP_NE,
	PY_CMP_GT,
	PY_CMP_GE,
	PY_CMP_IN,
	PY_CMP_NOT_IN,
	PY_CMP_IS,
	PY_CMP_IS_NOT,
	PY_CMP_EXC_MATCH,
	PY_CMP_BAD
};

#endif
