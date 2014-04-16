/* ??? At the moment, lower-subreg.c decomposes the copy of the multiplication
   result to $2, which prevents the register allocators from storing the
   multiplication result in $2.  */
/* { dg-options "-mips3 -mfix-r4000 -mgp64 -O2 -fno-split-wide-types -dp -EL" } */
typedef long long int64_t;
typedef int int128_t __attribute__((mode(TI)));
int128_t foo (int64_t x, int64_t y) { return (int128_t) x * y; }
/* { dg-final { scan-assembler "[concat {\tdmult\t\$[45],\$[45][^\n]+mulditi3_r4000[^\n]+\n\tmflo\t\$2\n\tmfhi\t\$3\n}]" } } */
