#include <limine/limine.h>
#include <stddef.h>
#include <stdint.h>

#include <dev/serial.h>
#include <stdio.h>

static void done(void)
{
	for (;;) {
		__asm__("hlt");
	}
}

void _start(void)
{
	serial_init();

	int test = printf("test %d", -304);
	printf("\nExpect: 9, actual: %d\n", test);
	
	done();
}
