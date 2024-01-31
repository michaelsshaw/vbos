/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <lib/stack.h>

slab_t *stack_slab;

void stack_init()
{
	stack_slab = slab_create(sizeof(struct stack_node), 16 * KB, 0);
}

struct stack *stack_create()
{
	struct stack *stack = kmalloc(sizeof(struct stack), ALLOC_KERN);
	if (!stack)
		return NULL;

	stack->top = NULL;

	return stack;
}

void stack_push(struct stack *stack, void *data)
{
	struct stack_node *node = slab_alloc(stack_slab);
	if (!node)
		return;

	node->data = data;
	node->next = stack->top;
	stack->top = node;
}

void *stack_pop(struct stack *stack)
{
	if (!stack->top)
		return NULL;

	struct stack_node *node = stack->top;
	stack->top = node->next;
	void *data = node->data;
	slab_free(stack_slab, node);

	return data;
}

void *stack_peek(struct stack *stack)
{
	if (!stack->top)
		return NULL;

	return stack->top->data;
}

void stack_destroy(struct stack *stack)
{
	while (stack->top)
		stack_pop(stack);

	kfree(stack);
}
