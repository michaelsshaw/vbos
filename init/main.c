/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>

#include <dev/apic.h>
#include <dev/serial.h>

#include <limine/limine.h>

static void done(void)
{
	for (;;) {
		__asm__("hlt");
	}
}

void kmain(void)
{
	gdt_init();
	idt_init();
	serial_init();
	acpi_init();
	apic_init();
	
	done();
}
