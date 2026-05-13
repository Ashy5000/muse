#ifndef USERSPACE_H
#define USERSPACE_H

#include <stdint.h>
#include "context.h"

void load_user_entry(func_ptr_t func_ptr);
void enter_ring3();

#endif
