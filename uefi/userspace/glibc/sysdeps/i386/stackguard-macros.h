#include <stdint.h>

#define STACK_CHK_GUARD \
  ({ uintptr_t x; asm ("movl %%gs:0x14, %0" : "=r" (x)); x; })
