/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <kernel/common.h>

#define RB_RED 0
#define RB_BLACK 1

struct rbnode {
	struct rbnode *parent;
	struct rbnode *left;
	struct rbnode *right;
	int color;

	uint64_t key;
	uint64_t value; /* extra value */
};

/* we use this rbtree structure because the root node will potentially be
 * changed when we insert or delete a node
 */
struct rbtree {
	struct rbnode *root;
};

struct rbnode *rbt_insert(struct rbtree *tree, uint64_t key);
void rbt_delete(struct rbtree *tree, struct rbnode *del);
struct rbnode *rbt_search(struct rbtree *tree, uint64_t key);
bool rbt_verify(struct rbtree *tree);
void rbt_slab_init();

#endif /* _RBTREE_H_ */
