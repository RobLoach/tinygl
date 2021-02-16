#include "zgl.h"

static char* op_table_str[] = {
#define ADD_OP(a, b, c) "gl" #a " " #c,

#include "opinfo.h"
};

static void (*op_table_func[])(GLContext*, GLParam*) = {
#define ADD_OP(a, b, c) glop##a,

#include "opinfo.h"
};

static GLint op_table_size[] = {
#define ADD_OP(a, b, c) b + 1,

#include "opinfo.h"
};

GLContext* gl_get_context(void) { return gl_ctx; }

static GLList* find_list(GLContext* c, GLuint list) { return c->shared_state.lists[list]; }

static void delete_list(GLContext* c, GLint list) {
	GLParamBuffer *pb, *pb1;
	GLList* l;

	l = find_list(c, list);
	if (l == NULL)
		puts("\nAttempted to delete NULL list!!!!\n");
	assert(l != NULL);

	/* free param buffer */
	pb = l->first_op_buffer;
	while (pb != NULL) {
		pb1 = pb->next;
		gl_free(pb);
		pb = pb1;
	}

	gl_free(l);
	c->shared_state.lists[list] = NULL;
}
void glDeleteLists(GLuint list, GLuint range) {
	for (GLuint i = 0; i < list + range; i++)
		glDeleteList(list + i);
}
void glDeleteList(GLuint list) { delete_list(gl_get_context(), list); }

static GLList* alloc_list(GLContext* c, GLint list) {
	GLList* l;
	GLParamBuffer* ob;

	l = gl_zalloc(sizeof(GLList));
	ob = gl_zalloc(sizeof(GLParamBuffer));

	ob->next = NULL;
	l->first_op_buffer = ob;

	ob->ops[0].op = OP_EndList;

	c->shared_state.lists[list] = l;
	return l;
}

void gl_print_op(FILE* f, GLParam* p) {
	GLint op;
	char* s;

	op = p[0].op;
	p++;
	s = op_table_str[op];
	while (*s != 0) {
		if (*s == '%') {
			s++;
			switch (*s++) {
			case 'f':
				fprintf(f, "%g", p[0].f);
				break;
			default:
				fprintf(f, "%d", p[0].i);
				break;
			}
			p++;
		} else {
			fputc(*s, f);
			s++;
		}
	}
	fprintf(f, "\n");
}

void gl_compile_op(GLContext* c, GLParam* p) {
	GLint op, op_size;
	GLParamBuffer *ob, *ob1;
	GLint index, i;

	op = p[0].op;
	op_size = op_table_size[op];
	index = c->current_op_buffer_index;
	ob = c->current_op_buffer;

	/* we should be able to add a NextBuffer opcode */
	if ((index + op_size) > (OP_BUFFER_MAX_SIZE - 2)) {

		ob1 = gl_zalloc(sizeof(GLParamBuffer));
		ob1->next = NULL;

		ob->next = ob1;
		ob->ops[index].op = OP_NextBuffer;
		ob->ops[index + 1].p = (void*)ob1;

		c->current_op_buffer = ob1;
		ob = ob1;
		index = 0;
	}

	for (i = 0; i < op_size; i++) {
		ob->ops[index] = p[i];
		index++;
	}
	c->current_op_buffer_index = index;
}

void gl_add_op(GLParam* p) {
	GLContext* c = gl_get_context();
	GLint op;

	op = p[0].op;
	if (c->exec_flag) {
		op_table_func[op](c, p);
	}
	if (c->compile_flag) {
		gl_compile_op(c, p);
	}
	if (c->print_flag) {
		gl_print_op(stderr, p);
	}
}

/* this opcode is never called directly */
void glopEndList(GLContext* c, GLParam* p) { assert(0); }

/* this opcode is never called directly */
void glopNextBuffer(GLContext* c, GLParam* p) { assert(0); }

void glopCallList(GLContext* c, GLParam* p) {
	GLList* l;
	GLint list, op;

	list = p[1].ui;
	l = find_list(c, list);
	if (l == NULL)
		gl_fatal_error("list %d not defined", list);
	p = l->first_op_buffer->ops;

	while (1) {
		op = p[0].op;
		if (op == OP_EndList)
			break;
		if (op == OP_NextBuffer) {
			p = (GLParam*)p[1].p;
		} else {
			op_table_func[op](c, p);
			p += op_table_size[op];
		}
	}
}

void glNewList(GLuint list, GLint mode) {
	GLList* l;
	GLContext* c = gl_get_context();

	assert(mode == GL_COMPILE || mode == GL_COMPILE_AND_EXECUTE);
	assert(c->compile_flag == 0);

	l = find_list(c, list);
	if (l != NULL)
		delete_list(c, list);
	l = alloc_list(c, list);

	c->current_op_buffer = l->first_op_buffer;
	c->current_op_buffer_index = 0;

	c->compile_flag = 1;
	c->exec_flag = (mode == GL_COMPILE_AND_EXECUTE);
}

void glEndList(void) {
	GLContext* c = gl_get_context();
	GLParam p[1];

	assert(c->compile_flag == 1);

	/* end of list */
	p[0].op = OP_EndList;
	gl_compile_op(c, p);

	c->compile_flag = 0;
	c->exec_flag = 1;
}

int glIsList(GLuint list) {
	GLContext* c = gl_get_context();
	GLList* l;
	l = find_list(c, list);
	return (l != NULL);
}

GLuint glGenLists(GLint range) {
	GLContext* c = gl_get_context();
	GLint count, i, list;
	GLList** lists;

	lists = c->shared_state.lists;
	count = 0;
	for (i = 0; i < MAX_DISPLAY_LISTS; i++) {
		if (lists[i] == NULL) {
			count++;
			if (count == range) {
				list = i - range + 1;
				for (i = 0; i < range; i++) {
					alloc_list(c, list + i);
				}
				return list;
			}
		} else {
			count = 0;
		}
	}
	return 0;
}
