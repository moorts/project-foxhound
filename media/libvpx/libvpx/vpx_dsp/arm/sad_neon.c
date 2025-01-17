/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/sum_neon.h"

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sadwxh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int w, int h) {
  // Only two accumulators are required for optimal instruction throughput of
  // the ABD, UDOT sequence on CPUs with either 2 or 4 Neon pipes.
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s0, s1, r0, r1, diff0, diff1;

      s0 = vld1q_u8(src_ptr + j);
      r0 = vld1q_u8(ref_ptr + j);
      diff0 = vabdq_u8(s0, r0);
      sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

      s1 = vld1q_u8(src_ptr + j + 16);
      r1 = vld1q_u8(ref_ptr + j + 16);
      diff1 = vabdq_u8(s1, r1);
      sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

      j += 32;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint32x4(vaddq_u32(sum[0], sum[1]));
}

static INLINE unsigned int sad64xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  return sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 64, h);
}

static INLINE unsigned int sad32xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  return sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 32, h);
}

static INLINE unsigned int sad16xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h / 2;
  do {
    uint8x16_t s0, s1, r0, r1, diff0, diff1;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    diff0 = vabdq_u8(s0, r0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;

    s1 = vld1q_u8(src_ptr);
    r1 = vld1q_u8(ref_ptr);
    diff1 = vabdq_u8(s1, r1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint32x4(vaddq_u32(sum[0], sum[1]));
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sad64xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  uint32x4_t sum_u32;

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, r0, r1, r2, r3;
    uint8x16_t diff0, diff1, diff2, diff3;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    diff0 = vabdq_u8(s0, r0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    diff1 = vabdq_u8(s1, r1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    diff2 = vabdq_u8(s2, r2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    diff3 = vabdq_u8(s3, r3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);

  return horizontal_add_uint32x4(sum_u32);
}

static INLINE unsigned int sad32xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint32x4_t sum = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t diff0 = vabdq_u8(s0, r0);
    uint16x8_t sum0 = vpaddlq_u8(diff0);

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t diff1 = vabdq_u8(s1, r1);
    uint16x8_t sum1 = vpaddlq_u8(diff1);

    sum = vpadalq_u16(sum, sum0);
    sum = vpadalq_u16(sum, sum1);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint32x4(sum);
}

static INLINE unsigned int sad16xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src_ptr);
    uint8x16_t r = vld1q_u8(ref_ptr);

    uint8x16_t diff = vabdq_u8(s, r);
    sum = vpadalq_u8(sum, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sad8xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x8_t s = vld1_u8(src_ptr);
    uint8x8_t r = vld1_u8(ref_ptr);

    sum = vabal_u8(sum, s, r);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

static INLINE unsigned int sad4xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h / 2;
  do {
    uint32x2_t s, r;
    uint32_t s0, s1, r0, r1;

    memcpy(&s0, src_ptr, 4);
    memcpy(&r0, ref_ptr, 4);
    s = vdup_n_u32(s0);
    r = vdup_n_u32(r0);
    src_ptr += src_stride;
    ref_ptr += ref_stride;

    memcpy(&s1, src_ptr, 4);
    memcpy(&r1, ref_ptr, 4);
    s = vset_lane_u32(s1, s, 1);
    r = vset_lane_u32(r1, r, 1);
    src_ptr += src_stride;
    ref_ptr += ref_stride;

    sum = vabal_u8(sum, vreinterpret_u8_u32(s), vreinterpret_u8_u32(r));
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

#define SAD_WXH_NEON(w, h)                                                   \
  unsigned int vpx_sad##w##x##h##_neon(const uint8_t *src, int src_stride,   \
                                       const uint8_t *ref, int ref_stride) { \
    return sad##w##xh_neon(src, src_stride, ref, ref_stride, (h));           \
  }

SAD_WXH_NEON(4, 4)
SAD_WXH_NEON(4, 8)

SAD_WXH_NEON(8, 4)
SAD_WXH_NEON(8, 8)
SAD_WXH_NEON(8, 16)

SAD_WXH_NEON(16, 8)
SAD_WXH_NEON(16, 16)
SAD_WXH_NEON(16, 32)

SAD_WXH_NEON(32, 16)
SAD_WXH_NEON(32, 32)
SAD_WXH_NEON(32, 64)

SAD_WXH_NEON(64, 32)
SAD_WXH_NEON(64, 64)

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sadwxh_avg_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int w, int h,
                                           const uint8_t *second_pred) {
  // Only two accumulators are required for optimal instruction throughput of
  // the ABD, UDOT sequence on CPUs with either 2 or 4 Neon pipes.
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s0, s1, r0, r1, p0, p1, avg0, avg1, diff0, diff1;

      s0 = vld1q_u8(src_ptr + j);
      r0 = vld1q_u8(ref_ptr + j);
      p0 = vld1q_u8(second_pred);
      avg0 = vrhaddq_u8(r0, p0);
      diff0 = vabdq_u8(s0, avg0);
      sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

      s1 = vld1q_u8(src_ptr + j + 16);
      r1 = vld1q_u8(ref_ptr + j + 16);
      p1 = vld1q_u8(second_pred + 16);
      avg1 = vrhaddq_u8(r1, p1);
      diff1 = vabdq_u8(s1, avg1);
      sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

      j += 32;
      second_pred += 32;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_uint32x4(vaddq_u32(sum[0], sum[1]));
}

static INLINE unsigned int sad64xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  return sadwxh_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride, 64, h,
                         second_pred);
}

static INLINE unsigned int sad32xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  return sadwxh_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride, 32, h,
                         second_pred);
}

static INLINE unsigned int sad16xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h / 2;
  do {
    uint8x16_t s0, s1, r0, r1, p0, p1, avg0, avg1, diff0, diff1;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    p0 = vld1q_u8(second_pred);
    avg0 = vrhaddq_u8(r0, p0);
    diff0 = vabdq_u8(s0, avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;

    s1 = vld1q_u8(src_ptr);
    r1 = vld1q_u8(ref_ptr);
    p1 = vld1q_u8(second_pred);
    avg1 = vrhaddq_u8(r1, p1);
    diff1 = vabdq_u8(s1, avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;
  } while (--i != 0);

  return horizontal_add_uint32x4(vaddq_u32(sum[0], sum[1]));
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sad64xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  uint32x4_t sum_u32;

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, r0, r1, r2, r3, p0, p1, p2, p3;
    uint8x16_t avg0, avg1, avg2, avg3, diff0, diff1, diff2, diff3;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    p0 = vld1q_u8(second_pred);
    avg0 = vrhaddq_u8(r0, p0);
    diff0 = vabdq_u8(s0, avg0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    p1 = vld1q_u8(second_pred + 16);
    avg1 = vrhaddq_u8(r1, p1);
    diff1 = vabdq_u8(s1, avg1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    p2 = vld1q_u8(second_pred + 32);
    avg2 = vrhaddq_u8(r2, p2);
    diff2 = vabdq_u8(s2, avg2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    p3 = vld1q_u8(second_pred + 48);
    avg3 = vrhaddq_u8(r3, p3);
    diff3 = vabdq_u8(s3, avg3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 64;
  } while (--i != 0);

  sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);

  return horizontal_add_uint32x4(sum_u32);
}

static INLINE unsigned int sad32xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint32x4_t sum = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t avg0 = vrhaddq_u8(r0, p0);
    uint8x16_t diff0 = vabdq_u8(s0, avg0);
    uint16x8_t sum0 = vpaddlq_u8(diff0);

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t avg1 = vrhaddq_u8(r1, p1);
    uint8x16_t diff1 = vabdq_u8(s1, avg1);
    uint16x8_t sum1 = vpaddlq_u8(diff1);

    sum = vpadalq_u16(sum, sum0);
    sum = vpadalq_u16(sum, sum1);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 32;
  } while (--i != 0);

  return horizontal_add_uint32x4(sum);
}

static INLINE unsigned int sad16xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src_ptr);
    uint8x16_t r = vld1q_u8(ref_ptr);
    uint8x16_t p = vld1q_u8(second_pred);

    uint8x16_t avg = vrhaddq_u8(r, p);
    uint8x16_t diff = vabdq_u8(s, avg);
    sum = vpadalq_u8(sum, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int sad8xh_avg_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h,
                                           const uint8_t *second_pred) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x8_t s = vld1_u8(src_ptr);
    uint8x8_t r = vld1_u8(ref_ptr);
    uint8x8_t p = vld1_u8(second_pred);

    uint8x8_t avg = vrhadd_u8(r, p);
    sum = vabal_u8(sum, s, avg);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 8;
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

static INLINE unsigned int sad4xh_avg_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h,
                                           const uint8_t *second_pred) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h / 2;
  do {
    uint32x2_t s, r;
    uint32_t s0, s1, r0, r1;
    uint8x8_t p, avg;

    memcpy(&s0, src_ptr, 4);
    memcpy(&r0, ref_ptr, 4);
    s = vdup_n_u32(s0);
    r = vdup_n_u32(r0);
    src_ptr += src_stride;
    ref_ptr += ref_stride;

    memcpy(&s1, src_ptr, 4);
    memcpy(&r1, ref_ptr, 4);
    s = vset_lane_u32(s1, s, 1);
    r = vset_lane_u32(r1, r, 1);
    src_ptr += src_stride;
    ref_ptr += ref_stride;

    p = vld1_u8(second_pred);
    avg = vrhadd_u8(vreinterpret_u8_u32(r), p);

    sum = vabal_u8(sum, vreinterpret_u8_u32(s), avg);
    second_pred += 8;
  } while (--i != 0);

  return horizontal_add_uint16x8(sum);
}

#define SAD_WXH_AVG_NEON(w, h)                                             \
  uint32_t vpx_sad##w##x##h##_avg_neon(const uint8_t *src, int src_stride, \
                                       const uint8_t *ref, int ref_stride, \
                                       const uint8_t *second_pred) {       \
    return sad##w##xh_avg_neon(src, src_stride, ref, ref_stride, (h),      \
                               second_pred);                               \
  }

SAD_WXH_AVG_NEON(4, 4)
SAD_WXH_AVG_NEON(4, 8)

SAD_WXH_AVG_NEON(8, 4)
SAD_WXH_AVG_NEON(8, 8)
SAD_WXH_AVG_NEON(8, 16)

SAD_WXH_AVG_NEON(16, 8)
SAD_WXH_AVG_NEON(16, 16)
SAD_WXH_AVG_NEON(16, 32)

SAD_WXH_AVG_NEON(32, 16)
SAD_WXH_AVG_NEON(32, 32)
SAD_WXH_AVG_NEON(32, 64)

SAD_WXH_AVG_NEON(64, 32)
SAD_WXH_AVG_NEON(64, 64)
