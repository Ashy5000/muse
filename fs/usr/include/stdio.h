#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include "size_t.h"

#define SEEK_SET 0

typedef struct { int fd; } FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stderr;
#define stderr stderr
int fclose(FILE*);
int fflush(FILE*);
FILE *fopen(const char*, const char*);
int fprintf(FILE*, const char*, ...);
size_t fread(void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
void setbuf(FILE*, char*);
int vfprintf(FILE*, const char*, va_list);
int sprintf(char *, const char *, ...);
int feof(FILE*);

#ifdef __cplusplus
}
#endif
#endif
