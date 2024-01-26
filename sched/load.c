/* SPDX-License-Identifier: GPL-2.0-only */
/* ELF loader */
#include <kernel/elf.h>
#include <kernel/slab.h>
#include <kernel/proc.h>
#include <kernel/gdt.h>

#include <fs/vfs.h>
#include <string.h>

static const char elf_magic[] = { 0x7F, 'E', 'L', 'F' };

int elf_check_compat(char *buf)
{
	if (strncmp(buf, elf_magic, 4))
		return -ENOEXEC;

	struct elf64_header *hdr = (struct elf64_header *)buf;

	/* must be 64-bit */
	if (hdr->e_ident[EI_CLASS] != ELFCLASS64)
		return -ENOEXEC;

	/* must be little endian */
	if (hdr->e_ident[EI_DATA] != ELFDATA2LSB)
		return -ENOEXEC;

	/* must be executable */
	if (hdr->e_type != ET_EXEC)
		return -ENOEXEC;

	/* must be x86_64 */
	if (hdr->e_machine != EM_X86_64)
		return -ENOEXEC;

	return 0;
}

pid_t elf_load_proc(char *fname)
{
	int err;
	struct file *file = vfs_open(fname, &err);

	if (!file)
		return -ENOENT;

	size_t size = file->vnode->size;

	char *buf = kmalloc(size, ALLOC_KERN);
	if (!buf)
		return -ENOMEM;

	vfs_read(file, buf, 0, size);
	vfs_close(file);

	int ret;

	if ((ret = elf_check_compat(buf))) {
		kfree(buf);
		return ret;
	}

	struct elf64_header *hdr = (struct elf64_header *)buf;

	/* read data and text segments */

	struct proc *proc = proc_create();
	/* allocate page tables */
	proc->cr3 = (uintptr_t)buddy_alloc(0x1000);

	/* map kernel pages */
	memcpy((void *)proc->cr3, (void *)(kcr3 | hhdm_start), 0x1000);

	proc->cr3 &= (~hhdm_start);

	struct elf64_program_header *phdr = (struct elf64_program_header *)(buf + hdr->e_phoff);
	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;

		uintptr_t vaddr = phdr[i].p_vaddr;
		size_t len = MAX(0x1000, npow2(phdr[i].p_filesz));
		uintptr_t paddr = (uintptr_t)buddy_alloc(len);

		/* length to copy, not allocated length */
		len = phdr[i].p_filesz;
		uint64_t flags = PAGE_PRESENT | PAGE_USER;

		if (phdr[i].p_flags & PF_W)
			flags |= PAGE_RW;

		if (!(phdr[i].p_flags & PF_X))
			flags |= PAGE_XD;

		(void)proc_mmap(proc, paddr & (~hhdm_start), vaddr, len, flags);
		memcpy((void *)paddr + (vaddr & 0xFFF), buf + phdr[i].p_offset, len);
	}

	/* set entry point */
	proc->regs.rip = hdr->e_entry;
	proc->regs.rflags = 0x242;

	/* descriptor #7 is the user code segment */
	proc->regs.cs = 0x38 | 3;
	proc->regs.ss = 0x40 | 3;
	ds_write(0x40 | 3);

	/* allocate user stack */
	uintptr_t stack_buf = (uintptr_t)buddy_alloc(0x1000);
	uintptr_t u_stack_addr = 0x20002000;
	(void)proc_mmap(proc, stack_buf & (~hhdm_start), u_stack_addr, 0x1000, PAGE_RW | PAGE_PRESENT | PAGE_USER | PAGE_XD);

	/* set stack pointer */
	proc->regs.rsp = u_stack_addr + 0xFF0;

	kfree(buf);

	return proc->pid;
}
