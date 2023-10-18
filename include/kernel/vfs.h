/* SDPX-License-Identifier: GPL-2.0-only */
#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/common.h>
#include <kernel/block.h>

struct file {
    struct inode *f_inode;
    uint64_t pos;
    uint32_t flags;
};

#endif /* _VFS_H_ */
