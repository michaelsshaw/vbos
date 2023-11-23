/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>

static slab_t *rbt_slab;

void rbt_slab_init()
{
	rbt_slab = slab_create(sizeof(struct rbnode), 1 * MB, 0);
}

static void rbt_rotate_right(struct rbtree *tree, struct rbnode *node)
{
	struct rbnode *left = node->left;

	node->left = left->right;
	if (left->right != NULL)
		left->right->parent = node;

	left->parent = node->parent;

	if (node->parent == NULL)
		tree->root = left;
	else if (node == node->parent->right)
		node->parent->right = left;
	else
		node->parent->left = left;

	left->right = node;
	node->parent = left;
}

static void rbt_rotate_left(struct rbtree *tree, struct rbnode *node)
{
	struct rbnode *right = node->right;

	node->right = right->left;
	if (right->left != NULL)
		right->left->parent = node;

	right->parent = node->parent;

	if (node->parent == NULL) {
		tree->root = right;
	} else if (node == node->parent->left) {
		node->parent->left = right;
	} else {
		node->parent->right = right;
	}

	right->left = node;
	node->parent = right;
}

static struct rbnode *rbt_uncle(struct rbnode *node)
{
	if (node->parent == NULL || node->parent->parent == NULL)
		return NULL;

	if (node->parent == node->parent->parent->left)
		return node->parent->parent->right;
	else
		return node->parent->parent->left;
}

static void rbt_insert_fixup(struct rbtree *tree, struct rbnode *node)
{
	while (node->parent && node->parent->color == RB_RED) {
		if (node->parent->parent && node->parent == node->parent->parent->left) {
			struct rbnode *uncle = rbt_uncle(node);
			if (uncle && uncle->color == RB_RED) {
				uncle->color = RB_BLACK;
				node->parent->color = RB_BLACK;
				node->parent->parent->color = RB_RED;
				node = node->parent->parent;
			} else if (node == node->parent->right) {
				node = node->parent;
				rbt_rotate_left(tree, node);
			} else {
				node->parent->color = RB_BLACK;
				node->parent->parent->color = RB_RED;
				rbt_rotate_right(tree, node->parent->parent);
			}
		} else {
			struct rbnode *uncle = rbt_uncle(node);
			if (uncle && uncle->color == RB_RED) {
				uncle->color = RB_BLACK;
				node->parent->color = RB_BLACK;
				node->parent->parent->color = RB_RED;
				node = node->parent->parent;
			} else if (node == node->parent->left) {
				node = node->parent;
				rbt_rotate_right(tree, node);
			} else {
				node->parent->color = RB_BLACK;
				node->parent->parent->color = RB_RED;
				rbt_rotate_left(tree, node->parent->parent);
			}
		}
	}
	if (tree->root)
		tree->root->color = RB_BLACK;
}

struct rbnode *rbt_insert(struct rbtree *tree, uint64_t key)
{
	struct rbnode *node = tree->root;
	struct rbnode *parent = NULL;

	while (node != NULL) {
		parent = node;
		if (key < node->key) {
			node = node->left;
		} else if (key > node->key) {
			node = node->right;
		} else {
			return NULL;
		}
	}

	struct rbnode *new = slab_alloc(rbt_slab);
	if (new == NULL)
		return NULL;

	new->key = key;
	new->parent = parent;
	new->left = NULL;
	new->right = NULL;
	new->color = RB_RED;

	new->value = 0;
	new->value2 = 0;
	new->value3 = 0;

	spinlock_acquire(&tree->lock);

	if (parent == NULL)
		tree->root = new;
	else if (key < parent->key)
		parent->left = new;
	else
		parent->right = new;

	rbt_insert_fixup(tree, new);
	tree->num_nodes++;
	spinlock_release(&tree->lock);

	return new;
}

static void rbt_transplant(struct rbtree *tree, struct rbnode *old, struct rbnode *new)
{
	if (old->parent == NULL)
		tree->root = new;
	else if (old == old->parent->left)
		old->parent->left = new;
	else
		old->parent->right = new;

	if (new != NULL)
		new->parent = old->parent;
}

static void rbt_delete_fixup(struct rbtree *tree, struct rbnode *node)
{
	if (!node)
		return;

	struct rbnode *sibling = NULL;
	while (node != tree->root && node->color == RB_BLACK) {
		if (node == node->parent->left) {
			sibling = node->parent->right;

			if (!sibling || !sibling->left || !sibling->right) {
				node = node->parent;
				continue;
			}

			if (sibling && sibling->color == RB_RED) {
				sibling->color = RB_BLACK;
				node->parent->color = RB_RED;
				rbt_rotate_left(tree, node->parent);
				sibling = node->parent->right;
			}

			if ((!sibling->left || sibling->left->color == RB_BLACK) &&
			    (!sibling->right || sibling->right->color == RB_BLACK)) {
				sibling->color = RB_RED;
				node = node->parent;
				continue;
			} else {
				if (sibling->right->color == RB_BLACK) {
					sibling->left->color = RB_BLACK;
					sibling->color = RB_RED;
					rbt_rotate_right(tree, sibling);
					sibling = node->parent->right;
				}

				sibling->color = node->parent->color;
				node->parent->color = RB_BLACK;
				sibling->right->color = RB_BLACK;
				rbt_rotate_left(tree, node->parent);
				node = tree->root;
			}
		} else {
			if (!sibling || !sibling->right || !sibling->right) {
				node = node->parent;
				continue;
			}

			if (sibling && sibling->color == RB_RED) {
				sibling->color = RB_BLACK;
				node->parent->color = RB_RED;
				rbt_rotate_right(tree, node->parent);
				sibling = node->parent->right;
			}

			if ((!sibling->right || sibling->left->color == RB_BLACK) &&
			    (!sibling->right || sibling->right->color == RB_BLACK)) {
				sibling->color = RB_RED;
				node = node->parent;
				continue;
			} else {
				if (sibling->right->color == RB_BLACK) {
					sibling->right->color = RB_BLACK;
					sibling->color = RB_RED;
					rbt_rotate_right(tree, sibling);
					sibling = node->parent->right;
				}

				sibling->color = node->parent->color;
				node->parent->color = RB_BLACK;
				sibling->right->color = RB_BLACK;
				rbt_rotate_right(tree, node->parent);
				node = tree->root;
			}
		}
	}

	node->color = RB_BLACK;
	tree->root->color = RB_BLACK;
}

struct rbnode *rbt_minimum(struct rbnode *node)
{
	if(node == NULL)
		return NULL;
	/* find the leftmost node */
	while (node->left != NULL)
		node = node->left;

	return node;
}

void rbt_delete(struct rbtree *tree, struct rbnode *del)
{
	spinlock_acquire(&tree->lock);
	/* case 0: left is NULL */
	if (!del->left) {
		rbt_transplant(tree, del, del->right);
		rbt_delete_fixup(tree, del->right);
		slab_free(rbt_slab, del);

	}

	/* case 1: right is NULL */
	else if (!del->right) {
		rbt_transplant(tree, del, del->left);
		rbt_delete_fixup(tree, del->left);
		slab_free(rbt_slab, del);
	}

	/* case 2: neither is NULL */
	else {
		struct rbnode *y = rbt_minimum(del->right);
		struct rbnode *x = y->right;
		int orig_color = y->color;

		if (y->parent == del) {
			if (x)
				x->parent = y;
		} else {
			rbt_transplant(tree, y, y->right);
			y->right = del->right;
			if (y->right)
				y->right->parent = y;
		}

		rbt_transplant(tree, del, y);
		y->left = del->left;
		if (y->left)
			y->left->parent = y;
		y->color = del->color;

		if (orig_color == RB_BLACK)
			rbt_delete_fixup(tree, x);
	}

	tree->num_nodes--;
	spinlock_release(&tree->lock);
}

void rbt_destroy(struct rbtree *tree)
{
	if (!tree)
		return;

	while (tree->root)
		rbt_delete(tree, tree->root);
}

struct rbnode *rbt_range_val2(struct rbtree *tree, uint64_t sval, uint64_t len)
{
	/* find closest node to sval */
	struct rbnode *node = tree->root;
	struct rbnode *closest = NULL;

	while (node != NULL) {
		if (node->key == sval) {
			closest = node;
			break;
		} else if (node->key < sval) {
			closest = node;
			node = node->right;
		} else {
			node = node->left;
		}
	}

	if (closest == NULL)
		return NULL;

	/* check if closest node is in range */
	if (closest->key + closest->value2 > sval)
		return closest;

	/* find next node */
	node = rbt_successor(closest);
	if (node == NULL)
		return NULL;

	/* check if next node is in range */
	if (node->key < sval + len)
		return node;

	return NULL;
}

struct rbnode *rbt_search(struct rbtree *tree, uint64_t key)
{
	struct rbnode *node = tree->root;

	while (node != NULL) {
		if (key < node->key) {
			node = node->left;
		} else if (key > node->key) {
			node = node->right;
		} else {
			return node;
		}
	}

	return NULL;
}

struct rbnode *rbt_successor(struct rbnode *node)
{
	if (node->right != NULL) {
		return rbt_minimum(node->right);
	}

	struct rbnode *parent = node->parent;
	while (parent != NULL && node == parent->right) {
		node = parent;
		parent = parent->parent;
	}

	return parent;
}

uint64_t rbt_next_key(struct rbtree *tree)
{
	/* find first available key */
	struct rbnode *node = tree->root;

	if (node == NULL)
		return 0;

	while (node->left != NULL)
		node = node->left;

	while (true) {
		struct rbnode *next = rbt_successor(node);
		if (next == NULL) {
			return node->key + 1;
		} else {
			if (next->key != node->key + 1) {
				return node->key + 1;
			} else {
				node = next;
			}
		}
	}
}

void rbtree_print_diagram_fancy(struct rbtree *tree, struct rbnode *node, int indent)
{
	if (node == NULL) {
		kprintf("<empty tree>\n");
		return;
	}

	if (node->right != NULL) {
		rbtree_print_diagram_fancy(tree, node->right, indent + 4);
	}

	if (indent) {
		for (int i = 0; i < indent; i++) {
			kprintf(" ");
		}
	}

	if (node->right != NULL) {
		kprintf(" /\n");
		for (int i = 0; i < indent; i++) {
			kprintf(" ");
		}
	}
	kprintf("%s%X%s\n", node->color == RB_RED ? "\033[31m" : "\033[0m", node->key, "\033[0m");

	if (node->left != NULL) {
		for (int i = 0; i < indent; i++) {
			kprintf(" ");
		}
		kprintf(" \\\n");
		rbtree_print_diagram_fancy(tree, node->left, indent + 4);
	}
}
