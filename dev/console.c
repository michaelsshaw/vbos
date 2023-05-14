#include <kernel/common.h>
#include <kernel/pio.h>

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

void console_init()
{
	memset(console.line, 0, 512);
	console.resizemode = 0;
	kprintf(LOG_SUCCESS "Console ready\n");
}

bool console_ready()
{
	return (console.resizemode == 0);
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
		kprintf("%c", c);
	} else {
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

void console_write(char c)
{
	if (!console.resizemode) {
		console.cursorpos++;
		if (console.cursorpos == console.cols)
			console_write('\n');
		if (c == '\n')
			console.cursorpos = 1;

		if (console.l_line < (sizeof(console.line) - 1)) {
			console.line[console.l_line] = c;
			console.l_line++;
		}
	}
	serial_write(c);
}
