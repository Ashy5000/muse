#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include "size_t.h"

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void*, const void*, size_t);
void *memset(void*, int, size_t);
char *strcpy(char*, const char*);
size_t strlen(const char*);
char *strcat(char*, const char*);
char *strchr(const char*, int);

#ifdef __cplusplus
}
#endif
#endif
