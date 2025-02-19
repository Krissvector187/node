/* crc32_simd.c
 *
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */

#include "crc32_simd.h"

#if defined(CRC32_SIMD_SSE42_PCLMUL)

/*
 * crc32_sse42_simd_(): compute the crc32 of the buffer, where the buffer
 * length must be at least 64, and a multiple of 16. Based on:
 *
 * "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"
 *  V. Gopal, E. Ozturk, et al., 2009, http://intel.ly/2ySEwL0
 */

#include <emmintrin.h>
#include <smmintrin.h>
#include <wmmintrin.h>

uint32_t ZLIB_INTERNAL crc32_sse42_simd_(  /* SSE4.2+PCLMUL */
    const unsigned char *buf,
    z_size_t len,
    uint32_t crc)
{
    /*
     * Definitions of the bit-reflected domain constants k1,k2,k3, etc and
     * the CRC32+Barrett polynomials given at the end of the paper.
     */
    static const uint64_t zalign(16) k1k2[] = { 0x0154442bd4, 0x01c6e41596 };
    static const uint64_t zalign(16) k3k4[] = { 0x01751997d0, 0x00ccaa009e };
    static const uint64_t zalign(16) k5k0[] = { 0x0163cd6124, 0x0000000000 };
    static const uint64_t zalign(16) poly[] = { 0x01db710641, 0x01f7011641 };

    __m128i x0, x1, x2, x3, x4, x5, x6, x7, x8, y5, y6, y7, y8;

    /*
     * There's at least one block of 64.
     */
    x1 = _mm_loadu_si128((__m128i *)(buf + 0x00));
    x2 = _mm_loadu_si128((__m128i *)(buf + 0x10));
    x3 = _mm_loadu_si128((__m128i *)(buf + 0x20));
    x4 = _mm_loadu_si128((__m128i *)(buf + 0x30));

    x1 = _mm_xor_si128(x1, _mm_cvtsi32_si128(crc));

    x0 = _mm_load_si128((__m128i *)k1k2);

    buf += 64;
    len -= 64;

    /*
     * Parallel fold blocks of 64, if any.
     */
    while (len >= 64)
    {
        x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
        x6 = _mm_clmulepi64_si128(x2, x0, 0x00);
        x7 = _mm_clmulepi64_si128(x3, x0, 0x00);
        x8 = _mm_clmulepi64_si128(x4, x0, 0x00);

        x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
        x2 = _mm_clmulepi64_si128(x2, x0, 0x11);
        x3 = _mm_clmulepi64_si128(x3, x0, 0x11);
        x4 = _mm_clmulepi64_si128(x4, x0, 0x11);

        y5 = _mm_loadu_si128((__m128i *)(buf + 0x00));
        y6 = _mm_loadu_si128((__m128i *)(buf + 0x10));
        y7 = _mm_loadu_si128((__m128i *)(buf + 0x20));
        y8 = _mm_loadu_si128((__m128i *)(buf + 0x30));

        x1 = _mm_xor_si128(x1, x5);
        x2 = _mm_xor_si128(x2, x6);
        x3 = _mm_xor_si128(x3, x7);
        x4 = _mm_xor_si128(x4, x8);

        x1 = _mm_xor_si128(x1, y5);
        x2 = _mm_xor_si128(x2, y6);
        x3 = _mm_xor_si128(x3, y7);
        x4 = _mm_xor_si128(x4, y8);

        buf += 64;
        len -= 64;
    }

    /*
     * Fold into 128-bits.
     */
    x0 = _mm_load_si128((__m128i *)k3k4);

    x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
    x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(x1, x2);
    x1 = _mm_xor_si128(x1, x5);

    x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
    x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(x1, x3);
    x1 = _mm_xor_si128(x1, x5);

    x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
    x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
    x1 = _mm_xor_si128(x1, x4);
    x1 = _mm_xor_si128(x1, x5);

    /*
     * Single fold blocks of 16, if any.
     */
    while (len >= 16)
    {
        x2 = _mm_loadu_si128((__m128i *)buf);

        x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
        x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
        x1 = _mm_xor_si128(x1, x2);
        x1 = _mm_xor_si128(x1, x5);

        buf += 16;
        len -= 16;
    }

    /*
     * Fold 128-bits to 64-bits.
     */
    x2 = _mm_clmulepi64_si128(x1, x0, 0x10);
    x3 = _mm_setr_epi32(~0, 0, ~0, 0);
    x1 = _mm_srli_si128(x1, 8);
    x1 = _mm_xor_si128(x1, x2);

    x0 = _mm_loadl_epi64((__m128i*)k5k0);

    x2 = _mm_srli_si128(x1, 4);
    x1 = _mm_and_si128(x1, x3);
    x1 = _mm_clmulepi64_si128(x1, x0, 0x00);
    x1 = _mm_xor_si128(x1, x2);

    /*
     * Barret reduce to 32-bits.
     */
    x0 = _mm_load_si128((__m128i*)poly);

    x2 = _mm_and_si128(x1, x3);
    x2 = _mm_clmulepi64_si128(x2, x0, 0x10);
    x2 = _mm_and_si128(x2, x3);
    x2 = _mm_clmulepi64_si128(x2, x0, 0x00);
    x1 = _mm_xor_si128(x1, x2);

    /*
     * Return the crc32.
     */
    return _mm_extract_epi32(x1, 1);
}

#elif defined(CRC32_ARMV8_CRC32)

/* CRC32 checksums using ARMv8-a crypto instructions.
 */

#if defined(__clang__)
/* CRC32 intrinsics are #ifdef'ed out of arm_acle.h unless we build with an
 * armv8 target, which is incompatible with ThinLTO optimizations on Android.
 * (Namely, mixing and matching different module-level targets makes ThinLTO
 * warn, and Android defaults to armv7-a. This restriction does not apply to
 * function-level `target`s, however.)
 *
 * Since we only need four crc intrinsics, and since clang's implementation of
 * those are just wrappers around compiler builtins, it's simplest to #define
 * those builtins directly. If this #define list grows too much (or we depend on
 * an intrinsic that isn't a trivial wrapper), we may have to find a better way
 * to go about this.
 *
 * NOTE: clang currently complains that "'+soft-float-abi' is not a recognized
 * feature for this target (ignoring feature)." This appears to be a harmless
 * bug in clang.
 */
/* XXX: Cannot hook into builtins with XCode for arm64. */
#if !defined(ARMV8_OS_MACOS)
#define __crc32b __builtin_arm_crc32b
#define __crc32d __builtin_arm_crc32d
#define __crc32w __builtin_arm_crc32w
#define __crc32cw __builtin_arm_crc32cw
#endif

/* We need some extra types for using PMULL.
 */
#if defined(__aarch64__)
#include <arm_neon.h>
#include <arm_acle.h>
#endif

#if defined(__aarch64__)
#define TARGET_ARMV8_WITH_CRC __attribute__((target("aes,crc")))
#else  // !defined(__aarch64__)
#define TARGET_ARMV8_WITH_CRC __attribute__((target("armv8-a,crc")))
#endif  // defined(__aarch64__)

#elif defined(__GNUC__)
/* For GCC, we are setting CRC extensions at module level, so ThinLTO is not
 * allowed. We can just include arm_acle.h.
 */
#include <arm_acle.h>
#define TARGET_ARMV8_WITH_CRC
#else  // !defined(__GNUC__) && !defined(_aarch64__)
#error ARM CRC32 SIMD extensions only supported for Clang and GCC
#endif

TARGET_ARMV8_WITH_CRC
uint32_t ZLIB_INTERNAL armv8_crc32_little(
    const unsigned char *buf,
    z_size_t len,
    uint32_t crc)
{
    uint32_t c = (uint32_t) ~crc;

    while (len && ((uintptr_t)buf & 7)) {
        c = __crc32b(c, *buf++);
        --len;
    }

    const uint64_t *buf8 = (const uint64_t *)buf;

    while (len >= 64) {
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);

        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        len -= 64;
    }

    while (len >= 8) {
        c = __crc32d(c, *buf8++);
        len -= 8;
    }

    buf = (const unsigned char *)buf8;

    while (len--) {
        c = __crc32b(c, *buf++);
    }

    return ~c;
}

#if defined(__aarch64__) || defined(ARMV8_OS_MACOS) /* aarch64 specific code. */

/*
 * crc32_pmull_simd_(): compute the crc32 of the buffer, where the buffer
 * length must be at least 64, and a multiple of 16. Based on:
 *
 * "Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"
 *  V. Gopal, E. Ozturk, et al., 2009, http://intel.ly/2ySEwL0
 */
TARGET_ARMV8_WITH_CRC
static inline uint8x16_t pmull_lo(const uint64x2_t a, const uint64x2_t b)
{
    uint8x16_t r;
    __asm__ __volatile__ ("pmull  %0.1q, %1.1d, %2.1d \n\t"
        : "=w" (r) : "w" (a), "w" (b) );
    return r;
}

TARGET_ARMV8_WITH_CRC
static inline uint8x16_t pmull_01(const uint64x2_t a, const uint64x2_t b)
{
    uint8x16_t r;
    __asm__ __volatile__ ("pmull  %0.1q, %1.1d, %2.1d \n\t"
        : "=w" (r) : "w" (a), "w" (vgetq_lane_u64(b, 1)) );
    return r;
}

TARGET_ARMV8_WITH_CRC
static inline uint8x16_t pmull_hi(const uint64x2_t a, const uint64x2_t b)
{
    uint8x16_t r;
    __asm__ __volatile__ ("pmull2 %0.1q, %1.2d, %2.2d \n\t"
        : "=w" (r) : "w" (a), "w" (b) );
    return r;
}

TARGET_ARMV8_WITH_CRC
uint32_t ZLIB_INTERNAL armv8_crc32_pmull_little(
    const unsigned char *buf,
    z_size_t len,
    uint32_t crc)
{
    /*
     * Definitions of the bit-reflected domain constants k1,k2,k3, etc and
     * the CRC32+Barrett polynomials given at the end of the paper.
     */
    static const uint64_t zalign(16) k1k2[] = { 0x0154442bd4, 0x01c6e41596 };
    static const uint64_t zalign(16) k3k4[] = { 0x01751997d0, 0x00ccaa009e };
    static const uint64_t zalign(16) k5k0[] = { 0x0163cd6124, 0x0000000000 };
    static const uint64_t zalign(16) poly[] = { 0x01db710641, 0x01f7011641 };

    uint64x2_t x0, x1, x2, x3, x4, x5, x6, x7, x8, y5, y6, y7, y8;

    /*
     * There's at least one block of 64.
     */
    x1 = vld1q_u64((const uint64_t *)(buf + 0x00));
    x2 = vld1q_u64((const uint64_t *)(buf + 0x10));
    x3 = vld1q_u64((const uint64_t *)(buf + 0x20));
    x4 = vld1q_u64((const uint64_t *)(buf + 0x30));

    x1 = veorq_u64(x1, (uint64x2_t) vsetq_lane_u32(crc, vdupq_n_u32(0), 0));

    x0 = vld1q_u64(k1k2);

    buf += 64;
    len -= 64;

    /*
     * Parallel fold blocks of 64, if any.
     */
    while (len >= 64)
    {
        x5 = (uint64x2_t) pmull_lo(x1, x0);
        x6 = (uint64x2_t) pmull_lo(x2, x0);
        x7 = (uint64x2_t) pmull_lo(x3, x0);
        x8 = (uint64x2_t) pmull_lo(x4, x0);

        y5 = vld1q_u64((const uint64_t *)(buf + 0x00));
        y6 = vld1q_u64((const uint64_t *)(buf + 0x10));
        y7 = vld1q_u64((const uint64_t *)(buf + 0x20));
        y8 = vld1q_u64((const uint64_t *)(buf + 0x30));

        x1 = (uint64x2_t) pmull_hi(x1, x0);
        x2 = (uint64x2_t) pmull_hi(x2, x0);
        x3 = (uint64x2_t) pmull_hi(x3, x0);
        x4 = (uint64x2_t) pmull_hi(x4, x0);

        x1 = veorq_u64(x1, x5);
        x2 = veorq_u64(x2, x6);
        x3 = veorq_u64(x3, x7);
        x4 = veorq_u64(x4, x8);

        x1 = veorq_u64(x1, y5);
        x2 = veorq_u64(x2, y6);
        x3 = veorq_u64(x3, y7);
        x4 = veorq_u64(x4, y8);

        buf += 64;
        len -= 64;
    }

    /*
     * Fold into 128-bits.
     */
    x0 = vld1q_u64(k3k4);

    x5 = (uint64x2_t) pmull_lo(x1, x0);
    x1 = (uint64x2_t) pmull_hi(x1, x0);
    x1 = veorq_u64(x1, x2);
    x1 = veorq_u64(x1, x5);

    x5 = (uint64x2_t) pmull_lo(x1, x0);
    x1 = (uint64x2_t) pmull_hi(x1, x0);
    x1 = veorq_u64(x1, x3);
    x1 = veorq_u64(x1, x5);

    x5 = (uint64x2_t) pmull_lo(x1, x0);
    x1 = (uint64x2_t) pmull_hi(x1, x0);
    x1 = veorq_u64(x1, x4);
    x1 = veorq_u64(x1, x5);

    /*
     * Single fold blocks of 16, if any.
     */
    while (len >= 16)
    {
        x2 = vld1q_u64((const uint64_t *)buf);

        x5 = (uint64x2_t) pmull_lo(x1, x0);
        x1 = (uint64x2_t) pmull_hi(x1, x0);
        x1 = veorq_u64(x1, x2);
        x1 = veorq_u64(x1, x5);

        buf += 16;
        len -= 16;
    }

    /*
     * Fold 128-bits to 64-bits.
     */
    static uint32_t zalign(16) mask[] = { ~0u, 0u, ~0u, 0u };

    x2 = (uint64x2_t) pmull_01(x1, x0);
    x1 = (uint64x2_t) vextq_u8(vreinterpretq_u8_u64(x1), vdupq_n_u8(0), 8);
    x3 = (uint64x2_t) vld1q_u32(mask);
    x1 = veorq_u64(x1, x2);

    x0 = vld1q_u64(k5k0);

    x2 = (uint64x2_t) pmull_01(x2, x0);
    x2 = (uint64x2_t) vextq_u8(vreinterpretq_u8_u64(x1), vdupq_n_u8(0), 4);
    x1 = vandq_u64(x1, x3);
    x1 = (uint64x2_t) pmull_lo(x1, x0);
    x1 = veorq_u64(x1, x2);

    /*
     * Barret reduce to 32-bits.
     */
    x0 = vld1q_u64(poly);

    x2 = vandq_u64(x1, x3);
    x2 = (uint64x2_t) pmull_01(x2, x0);
    x2 = vandq_u64(x2, x3);
    x2 = (uint64x2_t) pmull_lo(x2, x0);
    x1 = veorq_u64(x1, x2);

    /*
     * Return the crc32.
     */
    return vgetq_lane_u32(vreinterpretq_u32_u64(x1), 1);
}
#endif /* aarch64 specific code. */

#endif
