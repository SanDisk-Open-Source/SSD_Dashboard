/* { dg-lto-do run } */
/* { dg-lto-options { { -O0 -flto } } } */
/* { dg-extra-ld-options "-O2 -ffast-math -fuse-linker-plugin" } */
/* { dg-require-linker-plugin "" } */

/* We require a linker plugin because otherwise we'd need to link
   against libm which we are not sure here has cabs on all targets.
   This is because collect2 invokes ld on the -O0 object code
   which does not have folded cabs.  */

double cabs(_Complex double);
double __attribute__((used))
foo (_Complex double x, int b)
{
  if (b)
    x = 0;
  return cabs(x);
}
int main() { return 0; }
