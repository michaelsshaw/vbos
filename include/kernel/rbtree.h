/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RBTREE_H_
#define _RBTREE_H_

#include <kernel/common.h>
#include <kernel/lock.h>

#define RB_RED 0
#define RB_BLACK 1

struct rbnode {
	struct rbnode *parent;
	struct rbnode *left;
	struct rbnode *right;
	int color;

	uint64_t key;
	uint64_t value; /* extra values */
	uint64_t value2;
	uint64_t value3;
};

/* we use this rbtree structure because the root node will potentially be
 * changed when we insert or delete a node
 */
struct rbtree {
	struct rbnode *root;
	spinlock_t lock;
	size_t num_nodes;
};

struct rbnode *rbt_insert(struct rbtree *tree, uint64_t key);
void rbt_delete(struct rbtree *tree, struct rbnode *del);
void rbt_destroy(struct rbtree *tree);
struct rbnode *rbt_search(struct rbtree *tree, uint64_t key);
struct rbnode *rbt_successor(struct rbnode *node);
uint64_t rbt_next_key(struct rbtree *tree);
struct rbnode *rbt_range_val2(struct rbtree *tree, uint64_t sval, uint64_t len);
struct rbnode *rbt_minimum(struct rbnode *node);

void rbt_slab_init();

#endif /* _RBTREE_H_ */
