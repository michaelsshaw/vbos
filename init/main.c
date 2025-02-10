/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/pio.h>
#include <kernel/block.h>
#include <kernel/proc.h>
#include <kernel/trap.h>
#include <kernel/elf.h>

#include <dev/pic.h>
#include <dev/serial.h>
#include <dev/pci.h>
#include <dev/ahci.h>
#include <dev/apic.h>

#include <lib/stack.h>
#include <lib/sem.h>

#include <fs/vfs.h>
#include <fs/devfs.h>

#define LIMINE_INTERNAL_MODULE_REQUIRED (1 << 0)
#include <limine/limine.h>

struct limine_hhdm_request hhdm_req = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };
struct limine_kernel_file_request module_req = { .id = LIMINE_KERNEL_FILE_REQUEST, .revision = 0 };
struct limine_smp_request smp_req = { .id = LIMINE_SMP_REQUEST, .revision = 0 };

static char *kcmdline;
static char *kpath;
static struct limine_uuid kroot_disk;
static struct limine_uuid kroot_part;

uintptr_t hhdm_start;
uintptr_t data_end;
uintptr_t bss_start;
uintptr_t bss_end;

void gdt_load();
void idt_load();
void yield();
void ap_kmain(struct limine_smp_info *info);
void load_stack_and_jump(uintptr_t rsp, uintptr_t rbp, void *func, void *arg);

uintptr_t kstacks[256] = { 0 };

static inline void kexec(char *cmd)
{
	pid_t pid = elf_load_proc(cmd);

	if (pid < 0) {
		kprintf("Failed to load %s: %s\n", cmd, strerror(-pid));
		return;
	}

	kprintf("Loaded %s as PID %d\n", cmd, pid);

	struct proc *proc = proc_get(pid);
	if (!proc) {
		kprintf("Failed to get proc %d\n", pid);
		return;
	}

	void _return_to_user(struct procregs * regs, paddr_t cr3);
	proc_set_current(pid);
	_return_to_user(&proc->regs, proc->cr3);
}

void panic()
{
	kprintf(LOG_ERROR "KERNEL PANIC\n");
	cli();
	yield();
}

char *kcmdline_get_symbol(const char *sym)
{
	/* symbol=value */
	/* only parse until the space after the value */
	size_t sym_len = strlen(sym);
	size_t cmd_len = strlen(kcmdline);

	char *cmdline_copy = kmalloc(cmd_len + 1, ALLOC_KERN);
	if (!cmdline_copy) {
		return NULL;
	}

	strcpy(cmdline_copy, kcmdline);

	char *strtok_last = NULL;
	char *token = strtok(cmdline_copy, " ", &strtok_last);
	while (token) {
		if (!strncmp(token, sym, sym_len)) {
			size_t ret_len = strlen(token + sym_len + 1);
			char *ret = kzalloc(ret_len + 1, ALLOC_KERN);
			if (!ret) {
				kfree(cmdline_copy);
				return NULL;
			}

			strcpy(ret, token + sym_len + 1);

			kfree(cmdline_copy);
			return ret;
		}
		token = strtok(NULL, " ", &strtok_last);
	}

	return NULL;
}

/* kmain
 *
 * main kernel entry point, executed solely by the BSP
 */
void kmain()
{
	cli();
	hhdm_start = hhdm_req.response->offset;
	data_end = (uintptr_t)(&__data_end);
	bss_start = (uintptr_t)(&__bss_start);
	bss_end = (uintptr_t)(&__bss_end);

	/* Initialize early kernel memory array */
	const uintptr_t one_gb = 0x40000000;
	char kmem[one_gb] ALIGN(0x1000);
	mem_early_init(kmem, one_gb);

	stack_init();
	gdt_init();
	idt_init();

	struct limine_kernel_file_response *resp = module_req.response;

	struct limine_file *kfile = (struct limine_file *)((paddr_t)resp->kernel_file | hhdm_start);

	kcmdline = kzalloc(strlen(kfile->cmdline) + 1, ALLOC_KERN);
	strcpy(kcmdline, kfile->cmdline);

	kpath = kzalloc(strlen(kfile->path) + 1, ALLOC_KERN);
	strcpy(kpath, kfile->path);

	kroot_disk = kfile->gpt_disk_uuid;
	kroot_part = kfile->gpt_part_uuid;

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Kernel file: path='%s' cmdline='%s'\n", kpath, kcmdline);
	kprintf(LOG_DEBUG "Root partition: disk=%X-%X part=%X-%X\n", kroot_disk.a, kroot_disk.b, kroot_part.a, kroot_part.b);
#endif /* KDEBUG */

	acpi_init();

	pci_init();
	/* finished init, now load TSS */
#ifdef KDEBUG
	kprintf(LOG_DEBUG "Inserting TSS for BSP\n");
#endif

	void *kstack = buddy_alloc(KSTACK_SIZE);
	uintptr_t ptr = (uintptr_t)kstack + KSTACK_SIZE - 8;

	uint16_t tss = gdt_insert_tss(ptr);
	ltr(tss);

	/* init the application processors */
	struct limine_smp_response *smp_resp = smp_req.response;
	proc_init(smp_resp->cpu_count);
	void syscall_init();
	syscall_init();

	ahci_init();

	char *root_path = kcmdline_get_symbol("root");
	if (!root_path) {
		kprintf(LOG_ERROR "No root= parameter specified\n");

		kerror_print_blkdevs();
		panic();
	} else {
		vfs_init(root_path);
	}

	devfs_init();

	pic_init();
	apic_init();

	sem_t *init_sem = sem_create(0);

	for (unsigned i = 0; i < smp_resp->cpu_count; i++) {
		struct limine_smp_info *info = smp_resp->cpus[i];
		info->extra_argument = (uint64_t)init_sem;

		if (info->lapic_id == smp_resp->bsp_lapic_id)
			continue;

		info->goto_address = ap_kmain;
	}

	/* wait for the application processors to finish their initialization */
	for (unsigned i = 0; i < smp_resp->cpu_count - 1; i++) {
		while (sem_trywait(init_sem))
			(void)0;
	}

	sem_destroy(init_sem);
	exception_init();
	serial_init();
	irq_map(0, trap_sched);
	apic_enable_timer();

	load_stack_and_jump(ptr, ptr, kexec, "/bin/shtest.elf");
}

/* kmain for application processors
 *
 * This code enables basic processor functions and parks the processor, waiting
 * for future scheduling
 */
void ap_kmain(struct limine_smp_info *info)
{
	gdt_load();
	idt_load();
	cr3_write(kcr3);

	/* enable the APIC */
	lapic_enable();

	void *kstack = buddy_alloc(KSTACK_SIZE);
	uintptr_t ptr = (uintptr_t)kstack + KSTACK_SIZE - 8;

	kstacks[info->lapic_id] = (uintptr_t)ptr;

	/* insert the TSS for each processor into the GDT */
#ifdef KDEBUG
	kprintf(LOG_DEBUG "Inserting TSS for AP #%d\n", info->lapic_id);
#endif
	uint16_t tss = gdt_insert_tss(ptr);
	ltr(tss);

	sem_t *init_sem = (sem_t *)info->extra_argument;
	sem_post(init_sem);

	load_stack_and_jump(ptr, ptr, yield, NULL);
}
