/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/pio.h>
#include <kernel/cmd.h>

#include <dev/console.h>
#include <dev/serial.h>

static struct {
	int rows;
	int cols;
	int cursorpos;
	char line[512];
	size_t l_line;

	int resizemode;
	char resize_rows[8];
	char resize_cols[8];
	size_t l_resize_rows;
	size_t l_resize_cols;
} console;

static bool is_printable(char c)
{
	return (c >= 0x20 && c <= 0x7E);
}

void console_init()
{
	memset(console.line, 0, 512);
	console.resizemode = 0;
	console.l_line = 0;
	kprintf(LOG_SUCCESS "Console ready\n");
}

bool console_ready()
{
	return (console.resizemode == 0);
}

void console_clear()
{
	if (console.resizemode)
		return;

	kprintf("\e[2J\e[H");
	console.cursorpos = 0;
	return;
}

void console_resize()
{
	console.resizemode = 1;
	memset(console.resize_rows, 0, 8);
	memset(console.resize_cols, 0, 8);
	console.l_resize_rows = 0;
	console.l_resize_cols = 0;
	kprintf("\e[s"); /* save cursor position */

	kprintf("\e[999;999H"); /* move cursor FAR away */
	kprintf("\e[6n"); /* cursor position request */
	kprintf("\e[u"); /* restore cursor position */
	memset(console.line, 0, 512);
}

void console_input(char c)
{
	if (!console.resizemode) {
		if (is_printable(c)) {
			console_putchar(c);

			if (console.l_line < (sizeof(console.line) - 1)) {
				console.line[console.l_line] = c;
				console.l_line++;
			}
		} else if (c == '\n' || c == '\r') {
			console_putchar('\n');
			if (strlen(console.line) >= 4 && !memcmp("exec", console.line, 4)) {
				/* because this one doesn't return. i know it's bad, it will be changed later */
				void lapic_eoi();
				char line[512];
				strcpy(line, console.line);
				memset(console.line, 0, sizeof(console.line));
				console.l_line = 0;
				lapic_eoi();
				kexec(line);
				return;
			}
			kexec(console.line);
			memset(console.line, 0, sizeof(console.line));
			console.l_line = 0;
		} else if (c == 0x7F || c == '\b') {
			if (console.l_line > 0) {
				console_putchar('\b');
				console_putchar(' ');
				console_putchar('\b');
				console.l_line--;
				console.line[console.l_line] = 0;
			}
		}
	} else {
		/* TODO: replace with a proper state machine */
		if (c == 'R') {
			/* detect end of escape sequence */
			console.resizemode = 0;
			console.cols = atoi(console.resize_cols);
			console.rows = atoi(console.resize_rows);

			return;
		} else if (c == ';') {
			/* switch to cols when semicolon detected */
			console.resizemode += 1;
			return;
		} else if (c == '\e' || c == '[') {
			/* ignore the escape sequence */
			return;
		}
		char *out = console.resize_rows;
		size_t *l = &console.l_resize_rows;

		if (console.resizemode == 2) {
			out = console.resize_cols;
			l = &console.l_resize_cols;
		}

		out[*l] = c;
		*l = (*l + 1);
	}
}

void console_putchar(char c)
{
	if (!console.resizemode) {
		if (c == '\n')
			console.cursorpos = 0;
		if (console.cursorpos == console.cols && c != '\n')
			console_putchar('\n');
		console.cursorpos++;
	}
	serial_write(c);
}

ssize_t console_write(const char *buf, size_t count)
{
	for (size_t i = 0; i < count; i++)
		console_putchar(buf[i]);
	return count;
}

ssize_t console_read(char *buf, size_t count)
{
	if(console.l_line == 0)
		return -EAGAIN;

	return 0;
}
