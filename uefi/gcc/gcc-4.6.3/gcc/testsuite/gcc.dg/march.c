/* { dg-do compile } */
/* { dg-skip-if "" { *-*-* } { "-march=*" } { "" } } */
/* { dg-options "-march=foo" } */
/* { dg-error "march" "" { target *-*-* } 0 } */
/* { dg-bogus "mtune" "" { target *-*-* } 0 } */
int i;
