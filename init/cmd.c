/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/cmd.h>
#include <kernel/slab.h>

#include <dev/console.h>

#include <fs/vfs.h>

#include <string.h>

int kcmd_help(int argc, char **argv);
int kcmd_ls(int argc, char **argv);

#define KCMD_DECL(name)            \
	{                          \
		#name, kcmd_##name \
	}

struct kcmd {
	const char *name;
	int (*func)(int argc, char **argv);
};

struct kcmd cmd_list[] = { KCMD_DECL(help), KCMD_DECL(ls) };

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
