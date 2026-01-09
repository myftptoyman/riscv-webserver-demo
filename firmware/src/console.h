#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_printf(const char *fmt, ...);
void console_print_hex(unsigned long val);

#endif /* CONSOLE_H */
