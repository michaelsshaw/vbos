#include <kernel/common.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>

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
	
	done();
}
