/* { dg-options "-O2 -fipa-pta -fprofile-generate" } */
void *
memcpy (void *a, const void *b, __SIZE_TYPE__ len)
{
  if (a == b)
    __builtin_abort ();
}
