#ifndef USERSPACE_H
#define USERSPACE_H

#include <stdint.h>
#include "context.h"

void load_user_call_info(func_ptr_t func_ptr, uint32_t argc, char **argv);
void enter_ring3();

#endif
