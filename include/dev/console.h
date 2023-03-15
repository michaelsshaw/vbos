#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#ifndef __ASM__

void console_init();
void console_resize();
void console_input(char c);
bool console_ready();

#endif /* __ASM__ */
#endif /* _CONSOLE_H_ */
