/* { dg-do compile } */
/* The pic register save adds unavoidable stack pointer references.  */
/* { dg-skip-if "" { ilp32 && { ! nonpic } }  { "*" } { "" } } */
/* These options are selected to ensure 1 word needs to be allocated
   on the stack to maintain alignment for the call.  This should be
   transformed to push+pop.  We also want to force unwind info updates.  */
/* { dg-options "-Os -fomit-frame-pointer -fasynchronous-unwind-tables" } */
/* { dg-options "-Os -fomit-frame-pointer -mpreferred-stack-boundary=3 -fasynchronous-unwind-tables" { target ilp32 } } */

void f();
void g() { f(); f(); }

/* Both stack allocate and deallocate should be converted to push/pop.  */
/* { dg-final { scan-assembler-not "sp" } } */
