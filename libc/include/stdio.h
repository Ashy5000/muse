#ifndef _STDIO_H
#define _STDIO_H 1

#include "size_t.h"
#include <stdarg.h>
#include <stddef.h>

#define EOF -1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ    8192 /* Same as glibc */
#define FOPEN_MAX 16

typedef unsigned int fpos_t;

typedef struct {
	int fd;
	fpos_t pos;
	char *bfr;
	size_t bfr_size;
} FILE;

extern FILE *stderr;

#ifdef __cplusplus
extern "C" {
#endif

#define stderr stderr
int fclose(FILE *);
int fflush(FILE *);
FILE *fopen(const char *, const char *);
int getc(FILE *);
int fprintf(FILE *, const char *, ...);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
void setbuf(FILE *, char *);
int vfprintf(FILE *, const char *, va_list);
int sprintf(char *, const char *, ...);
int feof(FILE *);

#ifdef __cplusplus
}
#endif
#endif
