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

void write(char *s)
{
	char c;
	while ((c = *s++)) {
		serial_write(c);
	}
}

// The following will be our kernel's entry point.
void _start(void)
{
	// Ensure we got a terminal
	serial_init();
	int test = printf("test %d", -304);
	printf("\nExpect: 9, actual: %d\n", test);
	// We're done, just hang...
	done();
}
