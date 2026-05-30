#ifndef USERSPACE_H
#define USERSPACE_H

#include "context.h"
#include <stddef.h>
#include <stdint.h>

typedef void *unsafe_ptr;

void load_user_call_info(func_ptr_t func_ptr, uint32_t argc, char **argv);
void enter_ring3();
char *clean_string(unsafe_ptr str);
void *clean_data(unsafe_ptr data, size_t size);

#endif
