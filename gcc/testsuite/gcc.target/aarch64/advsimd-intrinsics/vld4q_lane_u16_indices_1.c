#include <arm_neon.h>

/* { dg-do compile } */
/* { dg-skip-if "" { *-*-* } { "-fno-fat-lto-objects" } } */
/* { dg-excess-errors "" { xfail arm*-*-* } } */

uint16x8x4_t
f_vld4q_lane_u16 (uint16_t * p, uint16x8x4_t v)
{
  uint16x8x4_t res;
  /* { dg-error "lane 8 out of range 0 - 7" "" { xfail arm*-*-* } 0 } */
  res = vld4q_lane_u16 (p, v, 8);
  /* { dg-error "lane -1 out of range 0 - 7" "" { xfail arm*-*-* } 0 } */
  res = vld4q_lane_u16 (p, v, -1);
  return res;
}
