/* Check that we can use the save instruction to save $16, $17 and $31.  */
/* { dg-options "(-mips16) isa_rev>=1 -mabi=32 -O2" } */
/* { dg-skip-if "PR target/46610" { mips-sgi-irix6* } } */

void bar (void);

MIPS16 void
foo (void)
{
  bar ();
  asm volatile ("" ::: "$16", "$17");
}
/* { dg-final { scan-assembler "\tsave\t\[0-9\]*,\\\$16,\\\$17,\\\$31" } } */
/* { dg-final { scan-assembler "\trestore\t\[0-9\]*,\\\$16,\\\$17,\\\$31" } } */
