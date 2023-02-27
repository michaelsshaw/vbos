#include <kernel/common.h>
#include <kernel/gdt.h>
#include <dev/serial.h>

#include <limine/limine.h>

static void done(void)
{
	for (;;) {
		__asm__("hlt");
	}
}

void _start(void)
{
	gdt_init();
	serial_init();
	
	done();
}
