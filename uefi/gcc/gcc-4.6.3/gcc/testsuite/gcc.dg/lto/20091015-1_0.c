/* { dg-lto-do link } */
/* { dg-lto-options {{-fPIC -r -nostdlib -O2 -flto} {-fPIC -r -nostdlib -O2 -flto -flto-partition=1to1}} } */

#include "20091015-1_b.h"
void diagnostic_initialize (FILE **stream) { *stream = stderr; }
