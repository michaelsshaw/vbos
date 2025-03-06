/* SPDX-License-Identifier: GPL-2.0-only */
/* ELF loader */
#include <kernel/elf.h>
#include <kernel/slab.h>
#include <kernel/proc.h>
#include <kernel/gdt.h>

#include <fs/vfs.h>

static const char elf_magic[] = { 0x7F, 'E', 'L', 'F' };

int elf_check_compat(char *buf)
{
	if (strncmp(buf, elf_magic, 4))
		return -ENOEXEC;

	Elf64_Ehdr *hdr = (Elf64_Ehdr *)buf;

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

Elf64_Shdr *elf_shdr(Elf64_Ehdr *hdr)
{
	return (Elf64_Shdr *)((uintptr_t)hdr + hdr->e_shoff);
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

	Elf64_Ehdr *hdr = (Elf64_Ehdr *)buf;

	/* read data and text segments */

	struct proc *proc = proc_create();
	/* allocate page tables */
	spinlock_acquire(&proc->lock);
	proc->cr3 = (uintptr_t)buddy_alloc(0x1000);

	/* map kernel pages */
	memcpy((void *)proc->cr3, (void *)(kcr3 | hhdm_start), 0x1000);

	proc->cr3 &= (~hhdm_start);

	Elf64_Phdr *phdr = (Elf64_Phdr *)(buf + hdr->e_phoff);
	for (unsigned int i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type != PT_LOAD)
			continue;

		uintptr_t vaddr = phdr[i].p_vaddr;
		size_t len = MAX(0x1000, npow2(phdr[i].p_filesz));
		uintptr_t paddr = (uintptr_t)buddy_alloc(len);
		uint64_t flags = PAGE_PRESENT | PAGE_USER;

		if (phdr[i].p_flags & PF_W)
			flags |= PAGE_RW;

		if (!(phdr[i].p_flags & PF_X))
			flags |= PAGE_XD;

		(void)proc_mmap(proc, paddr & (~hhdm_start), vaddr, len, flags);
		memcpy((void *)paddr + (vaddr & 0xFFF), buf + phdr[i].p_offset, phdr[i].p_filesz);
	}

	Elf64_Shdr *shdr_list = (Elf64_Shdr *)(buf + hdr->e_shoff);
	Elf64_Shdr *shdr_str = &shdr_list[hdr->e_shstrndx];

	for (unsigned int i = 0; i < hdr->e_shnum; i++) {
		Elf64_Shdr *section = &shdr_list[i];
		char *name = buf + shdr_str->sh_offset + section->sh_name;
	}

	/* set entry point */
	proc->regs.rip = hdr->e_entry;
	proc->regs.rflags = 0x242;

	/* descriptor #7 is the user code segment */
	proc->regs.cs = GDT_SEGMENT_CODE_RING3 | 3;
	proc->regs.ss = GDT_SEGMENT_DATA_RING3 | 3;
	ds_write(GDT_SEGMENT_DATA_RING3 | 3);

	/* allocate user stack */
	proc->stack_start = USER_STACK_BASE;
	proc->stack_size = 0; /* umalloc will grow the stack */
	void *ustack = umalloc(proc, 0x10000, ALLOC_USER_STACK, 0);

	/* set stack pointer */
	proc->regs.rsp = proc->stack_start + proc->stack_size;
	proc->regs.rbp = proc->regs.rsp;

	kfree(buf);

	spinlock_release(&proc->lock);

	return proc->pid;
}
