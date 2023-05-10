/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>

static struct slab *rbt_slab;

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

	if (node->parent == NULL) {
		tree->root = left;
	} else if (node == node->parent->right) {
		node->parent->right = left;
	} else {
		node->parent->left = left;
	}

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
	/* Case 0: node is root */
	if ((uintptr_t)tree == (uintptr_t)node || node->parent == NULL) {
		node->color = RB_BLACK;
		return;
	}

	while (node->parent && node->parent->color == RB_RED) {
		/* Case 1: node's uncle is red */
		struct rbnode *uncle_node = rbt_uncle(node);

		/* NULL nodes are black */
		if (uncle_node && uncle_node->color == RB_RED) {
			uncle_node->color = RB_BLACK;
			node->parent->color = RB_BLACK;
			node->parent->parent->color = RB_RED;
			node = node->parent->parent;
		}

		/* required for further cases */
		if (!node->parent || !node->parent->parent)
			return;

		/* Case 2: node's uncle is black and node is a right child of a parent 
		 * that is a left child, or vice versa 
		 *
		 * This will also set the node equal to its former parent (new child) to
		 * fallthrough to case 3
		 */
		if (node == node->parent->right && node->parent == node->parent->parent->left) {
			rbt_rotate_left(tree, node->parent);
			node = node->left;
		} else if (node == node->parent->left && node->parent == node->parent->parent->right) {
			rbt_rotate_right(tree, node->parent);
			node = node->right;
		}

		/* Case 3: Node's uncle is black and node, parent and grandparent form
		 * a line
		 */

		/* check again because case 2 might have changed the parent */
		if (!node->parent || !node->parent->parent)
			return;

		if (node == node->parent->left && node->parent == node->parent->parent->left) {
			node->parent->color = RB_BLACK;
			node->parent->parent->color = RB_RED;
			rbt_rotate_right(tree, node->parent->parent);
		} else if (node == node->parent->right && node->parent == node->parent->parent->right) {
			node->parent->color = RB_BLACK;
			node->parent->parent->color = RB_RED;
			rbt_rotate_left(tree, node->parent->parent);
		}
	}
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

	if (parent == NULL) {
		tree->root = new;
	} else if (key < parent->key) {
		parent->left = new;
	} else {
		parent->right = new;
	}

	rbt_insert_fixup(tree, new);
	return new;
}

static void rbt_transplant(struct rbtree *tree, struct rbnode *old, struct rbnode *new)
{
	if (old->parent == NULL) {
		tree->root = new;
	} else if (old == old->parent->left) {
		old->parent->left = new;
	} else {
		old->parent->right = new;
	}

	if (new != NULL) {
		new->parent = old->parent;
	}
}

static void rbt_delete_fixup(struct rbtree *tree, struct rbnode *node)
{
	if (!node)
		return;

	struct rbnode *sibling;
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

			if (sibling->left->color == RB_BLACK && sibling->right->color == RB_BLACK) {
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
		}
	}

	node->color = RB_BLACK;
}

static struct rbnode *rbt_minimum(struct rbnode *node)
{
	/* find the leftmost node */
	while (node->left != NULL)
		node = node->left;

	return node;
}

void rbt_delete(struct rbtree *tree, struct rbnode *del)
{
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

static int rbt_black_height(struct rbtree *tree)
{
	struct rbnode *node = tree->root;
	int black_count = 0;

	while (node != NULL) {
		if (node->color == RB_BLACK)
			black_count++;

		node = node->left;
	}

	return black_count;
}

/* verify that the tree is a valid red-black tree */
bool rbt_verify(struct rbtree *tree)
{
	struct rbnode *node = tree->root;
	struct rbnode *prev = NULL;
	int black_count = 0;

	while (node != NULL) {
		if (node->left != NULL && node->left->parent != node)
			return false;

		if (node->right != NULL && node->right->parent != node)
			return false;

		if (node->color == RB_BLACK)
			black_count++;

		if (node->left != NULL && node->left->key > node->key)
			return false;

		if (node->right != NULL && node->right->key < node->key)
			return false;

		prev = node;
		node = node->left;
	}

	if (prev != NULL && prev != rbt_minimum(tree->root))
		return false;

	if (black_count != rbt_black_height(tree))
		return false;

	return true;
}
