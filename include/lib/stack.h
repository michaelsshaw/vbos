/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STACK_H_
#define _STACK_H_

#include <stddef.h>

struct stack_node {
	struct stack_node *next;
	void *data;
};

struct stack {
	struct stack_node *top;
	size_t size;
};

void stack_init();
struct stack *stack_create();
void stack_destroy(struct stack *stack);
void stack_push(struct stack *stack, void *data);
void *stack_pop(struct stack *stack);
void *stack_peek(struct stack *stack);

#endif /* _STACK_H_ */
