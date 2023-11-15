/* SPDX-License-Identifier: GPL-2.0-only */
/* ELF loader */
#include <kernel/elf.h>
#include <kernel/slab.h>
#include <kernel/proc.h>

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

pid_t elf_load_proc(const char *proc_path)
{
	int fd = open(proc_path, O_RDONLY);
	if (fd < 0)
		return fd;

	char *buf = kmalloc(4096, ALLOC_KERN);
	if (!buf)
		return -ENOMEM;

	int ret = read(fd, buf, 4096);

	if (ret < 0)
		return ret;

	ret = elf_check_compat(buf);

	if (ret < 0) {
		kfree(buf);
		close(fd);
		return ret;
	}

	close(fd);

	return 0;
}
