/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/cmd.h>
#include <kernel/slab.h>
#include <kernel/elf.h>

#include <dev/console.h>

#include <fs/vfs.h>

#include <string.h>

int kcmd_help(int argc, char **argv);
int kcmd_ls(int argc, char **argv);
int kcmd_clear(int argc, char **argv);
int kcmd_cat(int argc, char **argv);
int kcmd_stat(int argc, char **argv);
int kcmd_basename(int argc, char **argv);
int kcmd_dirname(int argc, char **argv);
int kcmd_mkdir(int argc, char **argv);
int kcmd_unlink(int argc, char **argv);
int kcmd_elf(int argc, char **argv);

#define KCMD_DECL(name)            \
	{                          \
		#name, kcmd_##name \
	}

struct kcmd {
	const char *name;
	int (*func)(int argc, char **argv);
};

struct kcmd cmd_list[] = { KCMD_DECL(basename), KCMD_DECL(cat),	  KCMD_DECL(dirname), KCMD_DECL(clear),	 KCMD_DECL(help),
			   KCMD_DECL(ls),	KCMD_DECL(mkdir), KCMD_DECL(stat),    KCMD_DECL(unlink), KCMD_DECL(elf) };

int kcmd_basename(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: basename <path>\n");
		return 1;
	}

	char *base = basename(argv[1]);
	kprintf("%s\n", base);

	return 0;
}

int kcmd_dirname(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: dirname <path>\n");
		return 1;
	}

	char *dir = dirname(argv[1]);
	if (!dir)
		kprintf("/\n");
	else
		kprintf("%s\n", dir);

	return 0;
}

int kcmd_cat(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: cat <file>\n");
		return 1;
	}

	int fd = open(argv[1], 0);
	if (fd < 0) {
		kprintf("Failed to open %s: %s\n", argv[1], strerror(-fd));
		return 1;
	}

	char buf[1024];
	char lastchar = 0;

	while (true) {
		int n = read(fd, buf, sizeof(buf));
		if (n < 0) {
			kprintf("Failed to read %s: %s\n", argv[1], strerror(-n));
			return 1;
		} else if (n == 0) {
			if (lastchar != '\n')
				kprintf("\n");
			break;
		}

		for (int i = 0; i < n; i++) {
			console_write(buf[i]);
		}

		lastchar = buf[n - 1];
	}

	close(fd);

	return 0;
}

int kcmd_clear(int argc, char **argv)
{
	console_clear();
	return 0;
}

int kcmd_help(int argc, char **argv)
{
	kprintf("Available commands:\n");
	for (size_t i = 0; i < sizeof(cmd_list) / sizeof(struct kcmd); i++) {
		kprintf("  %s\n", cmd_list[i].name);
	}
	return 0;
}

int kcmd_ls(int argc, char **argv)
{
	char *path;
	if (!argv[1] || strempty(argv[1]))
		path = "/";
	else
		path = argv[1];

	DIR *dir = opendir(path);
	if (!dir) {
		kprintf("Failed to open directory %s\n", path);
		return 1;
	}

	struct dirent *ent;
	while ((ent = readdir(dir))) {
		if (ent->name[0] == '.')
			continue;

		kprintf("%s\n", ent->name);
	}

	return 0;
}

int kcmd_mkdir(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: mkdir <path>\n");
		return 1;
	}

	int ret = mkdir(argv[1]);
	if (ret < 0) {
		kprintf("Failed to mkdir %s: %s\n", argv[1], strerror(-ret));
		return 1;
	}

	return 0;
}

int kcmd_stat(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: stat <file>\n");
		return 1;
	}

	int fd = open(argv[1], 0);

	struct statbuf st;
	int ret = statfd(fd, &st);
	if (ret < 0) {
		kprintf("Failed to stat %s: %s\n", argv[1], strerror(-ret));
		return 1;
	}

	kprintf("File: %s\n", argv[1]);
	kprintf("Size: %d\n", st.size);

	return 0;
}

int kcmd_unlink(int argc, char **argv)
{
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: unlink <file>\n");
		return 1;
	}

	int ret = unlink(argv[1]);
	if (ret < 0) {
		kprintf("Failed to unlink %s: %s\n", argv[1], strerror(-ret));
		return 1;
	}

	return 0;
}

int kcmd_elf(int argc, char **argv)
{
	/* open file and check magic */
	if (!argv[1] || strempty(argv[1])) {
		kprintf("Usage: elf <file>\n");
		return 1;
	}

	int fd = open(argv[1], 0);
	if (fd < 0) {
		kprintf("Failed to open %s: %s\n", argv[1], strerror(-fd));
		return 1;
	}

	char *buf = kmalloc(4096, ALLOC_KERN);
	if (!buf)
		return -ENOMEM;

	int ret = read(fd, buf, 4096);
	if (ret < 0) {
		kprintf("Failed to read %s: %s\n", argv[1], strerror(-ret));
		close(fd);
		return 1;
	}

	char elf_magic[] = { 0x7F, 'E', 'L', 'F' };
	if (strncmp(buf, elf_magic, 4)) {
		kprintf("Not an ELF file\n");
		close(fd);
		return 1;
	}

	struct elf64_header *hdr = (struct elf64_header *)buf;

	kprintf("ELF header:\n");
	kprintf("  Magic: %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);
	kprintf("  Class: %d\n", hdr->e_ident[EI_CLASS]);
	kprintf("  Data: %d\n", hdr->e_ident[EI_DATA]);
	kprintf("  Type: %d\n", hdr->e_type);
	kprintf("  Machine: %d\n", hdr->e_machine);
	kprintf("  Version: %d\n", hdr->e_version);
	kprintf("  Entry: %Xh\n", hdr->e_entry);
	kprintf("  Program header offset: %d\n", hdr->e_phoff);
	kprintf("  Section header offset: %d\n", hdr->e_shoff);
	kprintf("  Flags: %d\n", hdr->e_flags);
	kprintf("  Header size: %d\n", hdr->e_ehsize);
	kprintf("  Program header size: %d\n", hdr->e_phentsize);
	kprintf("  Program header count: %d\n", hdr->e_phnum);
	kprintf("  Section header size: %d\n", hdr->e_shentsize);
	kprintf("  Section header count: %d\n", hdr->e_shnum);
	kprintf("  Section header string table index: %d\n", hdr->e_shstrndx);

	close(fd);

	return 0;
}

void kexec(const char *cmd)
{
	int n;
	char **spl = strsplit(cmd, ' ', &n);

	if (!spl)
		return;

	if (!spl[0] || strempty(spl[0]))
		goto cleanup;

	/* do command */
	bool found = false;
	for (size_t i = 0; i < sizeof(cmd_list) / sizeof(struct kcmd); i++) {
		if (!strcmp(cmd_list[i].name, spl[0])) {
			cmd_list[i].func(n, spl);
			found = true;
			break;
		}
	}

	if (!found)
		kprintf("Command not found: '%s'\n", spl[0]);

cleanup:
	for (int i = 0; i < n; i++) {
		kfree(spl[i]);
	}
	kfree(spl);

	kprintf("# ");
}
