#ifndef TEXT_H
#define TEXT_H

void init_console(void);
void kscroll(void);
void kdel_char(void);
void kput_char(char c);
void kprint(const char *str);
void kprint_hex(char *data, int len);
void kprint_int(int x, int base);
void kprint_int_full(int x, int base);

#endif
