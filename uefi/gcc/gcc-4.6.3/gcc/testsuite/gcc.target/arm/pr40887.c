/* { dg-options "-O2 -march=armv5te" }  */
/* { dg-final { scan-assembler "blx" } } */

int (*indirect_func)();

int indirect_call()
{
    return indirect_func();
}
