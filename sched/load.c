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
	int fd = open(fname, O_RDONLY);
	if (fd < 0)
		return fd;

	seek(fd, 0, SEEK_END);
	size_t size = tell(fd);
	seek(fd, 0, SEEK_SET);

	char *buf = kmalloc(size, ALLOC_KERN);
	if (!buf)
		return -ENOMEM;

	read(fd, buf, size);
	close(fd);

	int ret;

	if ((ret = elf_check_compat(buf))) {
		kfree(buf);
		return ret;
	}

	struct elf64_header *hdr = (struct elf64_header *)buf;

	/* read data and text segments */

	struct proc *proc = proc_create();
	/* allocate page tables */
	proc->cr3 = (uintptr_t)buddy_alloc(0x1000) & (~hhdm_start);

	struct elf64_program_header *phdr = (struct elf64_program_header *)(buf + hdr->e_phoff);
	for (int i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD) {
			/* allocate memory for segment */
			size_t psize = MAX(0x1000, npow2(phdr[i].p_memsz));
			uintptr_t prog_buf = (uintptr_t)buddy_alloc(psize);
			void *addr = proc_mmap(proc, prog_buf & (~hhdm_start), phdr[i].p_vaddr, psize, PAGE_RW | PAGE_PRESENT);
			if (!addr) {
				kfree(buf);
				return -ENOMEM;
			}

			/* copy data from file to memory */
			memcpy((void *)prog_buf, buf + phdr[i].p_offset, phdr[i].p_filesz);

			/* zero out remaining memory */
			memset((void *)prog_buf + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
		}
	}

	/* set entry point */
	proc->regs.rip = hdr->e_entry;
	proc->regs.rflags = 0x202; /* interrupts enabled, IOPL = 0 */
	/* descriptor #7 is the user code segment */
	proc->regs.cs = 7 * sizeof(struct gdt_desc) | 0x3;
	proc->regs.ss = 8 * sizeof(struct gdt_desc) | 0x3;

	/* allocate user stack */
	uintptr_t stack_buf = (uintptr_t)buddy_alloc(0x1000);
	uintptr_t u_stack_addr = 0x202020202000;
	(void)proc_mmap(proc, stack_buf & (~hhdm_start), u_stack_addr, 0x1000, PAGE_RW | PAGE_PRESENT);

	/* set stack pointer */
	proc->regs.rsp = u_stack_addr + 0x1000;

	kfree(buf);

	return proc->pid;
}
