/* { dg-do compile } */
/* { dg-require-effective-target ilp32 } */
/* { dg-options "-O2 -march=i586 -ffast-math" } */

extern double host_frametime;
extern float pitchvel;
V_DriftPitch (float delta, float move)
{
  if (!delta)
    move = host_frametime;
  if (delta > 0)
    ;
  else if (delta < 0 && move > -delta)
    pitchvel = 0;
}
