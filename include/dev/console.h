#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#ifndef __ASM__

#include <stdbool.h>

void console_init();
void console_resize();
void console_input(char c);
void console_clear();
bool console_ready();
void console_write(char c);

#endif /* __ASM__ */
#endif /* _CONSOLE_H_ */
