#include <kernel/common.h>
#include <kernel/pio.h>

#include <dev/console.h>

static struct {
	int rows;
	int cols;
	int cursorpos;
	char line[512];

	int resizemode;
	char resize_rows[8];
	char resize_cols[8];
	int l_resize_rows;
	int l_resize_cols;
} console;

void console_init()
{
	memset(console.line, 0, 512);
	console.resizemode = false;
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
	printf("\e[s"); /* save cursor position */

	printf("\e[999;999H"); /* move cursor FAR away */
	printf("\e[6n"); /* cursor position request */
	printf("\e[u"); /* restore cursor position */
	memset(console.line, 0, 512);
}

void console_input(char c)
{
	if (!console.resizemode) {
		printf("%c", c);
	} else {
		if (c == 'R') {
			/* detect end of escape sequence */
			printf(LOG_SUCCESS "Console resize: Rows=%s, Cols=%s\n", console.resize_rows, console.resize_cols);
			console.resizemode = 0;
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
		int *l = &console.l_resize_rows;

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
	console.cursorpos += 1;
}
