/* { dg-options "-march=r4000 -mfix-r4000 -O2 -mgp64 -dp -EB" } */
typedef long long int64_t;
typedef int int128_t __attribute__((mode(TI)));
int64_t foo (int64_t x, int64_t y) { return ((int128_t) x * y) >> 64; }
/* ??? A highpart pattern would be a better choice, but we currently
   don't use them.  */
/* { dg-final { scan-assembler "[concat {\tdmult\t\$[45],\$[45][^\n]+mulditi3[^\n]+\n\tmflo\t\$3\n\tmfhi\t\$2\n}]" } } */
