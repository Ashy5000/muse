#ifndef USERSPACE_H
#define USERSPACE_H

#include <stdint.h>
#include "context.h"

void enter_ring3(func_ptr_t func_ptr);
#endif
