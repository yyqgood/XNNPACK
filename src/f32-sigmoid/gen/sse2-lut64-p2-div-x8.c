// Auto-generated file. Do not edit!
//   Template: src/f32-sigmoid/sse-lut64-p2-div.c.in
//   Generator: tools/xngen
//
// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <emmintrin.h>

#include <xnnpack/common.h>
#include <xnnpack/math.h>
#include <xnnpack/vunary.h>


extern XNN_INTERNAL const float xnn_table_exp2_k_over_64[64];

void xnn_f32_sigmoid_ukernel__sse2_lut64_p2_div_x8(
    size_t n,
    const float* x,
    float* y,
    const void* params) XNN_DISABLE_TSAN
{
  assert(n % sizeof(float) == 0);

  // Mask for all bits of a floating-point number except the sign bit.
  const __m128 vnonsign_mask = _mm_set1_ps(math_nonsign_mask_f32());

  const __m128 vmagic_bias = _mm_set1_ps(0x1.800000p23f);
  // The largest z for which sigmoidf(-z) is normalized.
  // This number is also the largest z for which expf(-z) is normalized.
  const __m128 vdenorm_cutoff = _mm_set1_ps(0x1.5D589Ep+6f);
  const __m128 vminus_log2e_x64 = _mm_set1_ps(-0x1.715476p6f);
  // Last 13 bits are zeroes
  const __m128 vln2_o64_hi = _mm_set1_ps(0x1.630000p-7f);
  const __m128 vln2_o64_lo = _mm_set1_ps(-0x1.BD0106p-19f);
  const __m128 vone = _mm_set1_ps(1.0f);

  const __m128 vc2 = _mm_set1_ps(0x1.FFFF0Ap-2f);

  const __m128i vinv_index_mask = _mm_set1_epi32(~INT32_C(0x3F));

  for (; n >= 8 * sizeof(float); n -= 8 * sizeof(float)) {
    const __m128 vx0123 = _mm_loadu_ps(x);
    const __m128 vx4567 = _mm_loadu_ps(x + 4);
    x += 8;

    // General structure of the algorithm:
    //           / exp(x) / (1 + exp(x)) if x <= 0
    //   f[x] :=
    //           \ 1 - f[-x] if x >= 0
    //
    // First we compute f[-z] := exp(-z) / (1 + exp(-z)) where z = abs(x),
    // then replace result with 1 - f[-z] if x >= 0.
    const __m128 vz0123 = _mm_and_ps(vx0123, vnonsign_mask);
    const __m128 vz4567 = _mm_and_ps(vx4567, vnonsign_mask);

    // Compute reduced argument n := round(-z * 64 / log(2)).
    // We do it by adding a large number (magic bias), which cause rounding of the result to an integer, then subtracing
    // the large number back. The first addition is combined with multiplication by log2e into a single FMA instruction.
    // The trick with adding large number is valid only within certain bounds (|z * 64 / log(2)| <= 2**22, i.e.
    // |z| <= 0x1.62E43p+15 = 45426.09375), but that is acceptable, because inputs x outside of [-87.336544, 17.328678]
    // (i.e. z outsize [0, 87.336544]) underflow or saturate sigmoidf(x). We fixup the result  for such inputs at the
    // very end of the algorithm.
    __m128 vn0123 = _mm_add_ps(vmagic_bias, _mm_mul_ps(vz0123, vminus_log2e_x64));
    __m128 vn4567 = _mm_add_ps(vmagic_bias, _mm_mul_ps(vz4567, vminus_log2e_x64));

    // Create a floating-point number s (scale) such that s := 2**(n / 64) for such inputs that sigmoidf(-z) is
    // normalized, i.e. 0 <= z <= 87.33642. As n has 6 fractional bits, we split s == 2**(n / 64) =
    // = 2**e * 2**(n / 64 - e), where e := int(n / 64). We create s in two steps:
    // 1. Fetch 2**(n / 64 - e) = 2**(n % 64) from the table using the 6 low bits of n, as integer. Note that the
    //    fetched values are in the [1.0, 2.0) range, i.e. their floating-point exponent is 0.
    // 2. Adjust fecthed value by addition of e to its floating-point exponent. The result is always a normalized
    //    number, because for 0 <= z <= 87.33642 (inputs for which sigmoidf(-z) is normalized) we have -126 <= e <= 0,
    //    and thus the adjusted exponent is not lower than -126.
    //
    // Extract e from bits 6:14 of n and shift it into bits 23:31 (position of floating-point exponent).
    const __m128i ve0123 = _mm_slli_epi32(_mm_and_si128(_mm_castps_si128(vn0123), vinv_index_mask), 17);
    const __m128i ve4567 = _mm_slli_epi32(_mm_and_si128(_mm_castps_si128(vn4567), vinv_index_mask), 17);

    // Use bits 0:6 bits of n, as integer, as an index for table lookup of l := 2**(n % 64).
    const __m128i vidx0123 = _mm_slli_epi32(_mm_andnot_si128(vinv_index_mask, _mm_castps_si128(vn0123)), 2);
    const __m128i vidx4567 = _mm_slli_epi32(_mm_andnot_si128(vinv_index_mask, _mm_castps_si128(vn4567)), 2);

    #if XNN_ARCH_X86_64
      const uint64_t vidx01 = (uint64_t) _mm_cvtsi128_si64(vidx0123);
      const uint64_t vidx23 = (uint64_t) _mm_cvtsi128_si64(_mm_unpackhi_epi64(vidx0123, vidx0123));
      const __m128i vl0   = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx01)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx23)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx01 >> 32))));
      const __m128i vl01 = _mm_unpacklo_epi32(vl0, vl1);
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx23 >> 32))));
      const __m128i vl23 = _mm_unpacklo_epi32(vl2, vl3);
      const __m128i vl0123 = _mm_unpacklo_epi64(vl01, vl23);
      const uint64_t vidx45 = (uint64_t) _mm_cvtsi128_si64(vidx4567);
      const uint64_t vidx67 = (uint64_t) _mm_cvtsi128_si64(_mm_unpackhi_epi64(vidx4567, vidx4567));
      const __m128i vl4   = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx45)));
      const __m128i vl6 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx67)));
      const __m128i vl5 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx45 >> 32))));
      const __m128i vl45 = _mm_unpacklo_epi32(vl4, vl5);
      const __m128i vl7 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx67 >> 32))));
      const __m128i vl67 = _mm_unpacklo_epi32(vl6, vl7);
      const __m128i vl4567 = _mm_unpacklo_epi64(vl45, vl67);
    #else  // !XNN_ARCH_X86_64
      const uint32_t vidx0 = (uint32_t) _mm_cvtsi128_si32(vidx0123);
      const uint32_t vidx1 = (uint32_t) _mm_extract_epi16(vidx0123, 2);
      const uint32_t vidx2 = (uint32_t) _mm_extract_epi16(vidx0123, 4);
      const uint32_t vidx3 = (uint32_t) _mm_extract_epi16(vidx0123, 6);
      const __m128i vl0   = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx0)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx2)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx1)));
      const __m128i vl01 = _mm_unpacklo_epi32(vl0, vl1);
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx3)));
      const __m128i vl23 = _mm_unpacklo_epi32(vl2, vl3);
      const __m128i vl0123 = _mm_unpacklo_epi64(vl01, vl23);
      const uint32_t vidx4 = (uint32_t) _mm_cvtsi128_si32(vidx4567);
      const uint32_t vidx5 = (uint32_t) _mm_extract_epi16(vidx4567, 2);
      const uint32_t vidx6 = (uint32_t) _mm_extract_epi16(vidx4567, 4);
      const uint32_t vidx7 = (uint32_t) _mm_extract_epi16(vidx4567, 6);
      const __m128i vl4   = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx4)));
      const __m128i vl6 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx6)));
      const __m128i vl5 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx5)));
      const __m128i vl45 = _mm_unpacklo_epi32(vl4, vl5);
      const __m128i vl7 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx7)));
      const __m128i vl67 = _mm_unpacklo_epi32(vl6, vl7);
      const __m128i vl4567 = _mm_unpacklo_epi64(vl45, vl67);
    #endif  // XNN_ARCH_X86_64

    // Adjust exponent of the value l fetched from the table to get the final s value.
    const __m128 vs0123 = _mm_castsi128_ps(_mm_add_epi32(vl0123, ve0123));
    const __m128 vs4567 = _mm_castsi128_ps(_mm_add_epi32(vl4567, ve4567));

    // Subtract the large number back to get the final n := round(-z * 64 / log(2)) as a floating-point number.
    vn0123 = _mm_sub_ps(vn0123, vmagic_bias);
    vn4567 = _mm_sub_ps(vn4567, vmagic_bias);

    // Compute reduced argument t := (z + n * log(2) / 64). Note that -t = -z - n * log(2) / 64.
    // Use Cody-Waite range reduction method (note two constants to represent log(2) / 64) to improve accuracy.
    __m128 vt0123 = _mm_add_ps(vz0123, _mm_mul_ps(vn0123, vln2_o64_hi));
    vt0123 = _mm_add_ps(vt0123, _mm_mul_ps(vn0123, vln2_o64_lo));
    __m128 vt4567 = _mm_add_ps(vz4567, _mm_mul_ps(vn4567, vln2_o64_hi));
    vt4567 = _mm_add_ps(vt4567, _mm_mul_ps(vn4567, vln2_o64_lo));

    // Compute degree-2 polynomial approxiatmion for exp(-t) on [-log(2)/128, log(2)/128].
    //   P1(t) = 1 + t * (-1 + t * c2)
    __m128 vp0123 = _mm_mul_ps(vt0123, vc2);
    vp0123 = _mm_sub_ps(vt0123, _mm_mul_ps(vp0123, vt0123));
    __m128 vp4567 = _mm_mul_ps(vt4567, vc2);
    vp4567 = _mm_sub_ps(vt4567, _mm_mul_ps(vp4567, vt4567));

    // Reconstruct the exp(-z) value:
    //   f = s * (1 + t * (-1 + t * c2))
    //     = s * (1 - t + t * (t * c2))
    //     = s - s * (t - t * (t * c2))
    //     = s - s * p
    const __m128 vy0123 = _mm_sub_ps(vs0123, _mm_mul_ps(vs0123, vp0123));
    const __m128 vy4567 = _mm_sub_ps(vs4567, _mm_mul_ps(vs4567, vp4567));

    // Reconstruct sigmoid(-z) = exp(-z) / (1.0 + exp(-z))
    __m128 vf0123 = _mm_div_ps(vy0123, _mm_add_ps(vy0123, vone));
    __m128 vf4567 = _mm_div_ps(vy4567, _mm_add_ps(vy4567, vone));

    // For inputs below denormal cutoff, replace output with +0.0f.
    // Note that for NaN inputs, comparison result is false, and outputs are left unchanged.
    vf0123 = _mm_andnot_ps(_mm_cmplt_ps(vdenorm_cutoff, vz0123), vf0123);
    vf4567 = _mm_andnot_ps(_mm_cmplt_ps(vdenorm_cutoff, vz4567), vf4567);

    // Reconstruct sigmoid(x) = x < 0 ? sigmoid(z) : 1.0 - sigmoid(z)
    __m128 vm0123 = _mm_castsi128_ps(_mm_cmpgt_epi32(_mm_setzero_si128(), _mm_castps_si128(vx0123)));
    __m128 vm4567 = _mm_castsi128_ps(_mm_cmpgt_epi32(_mm_setzero_si128(), _mm_castps_si128(vx4567)));

    vf0123 = _mm_or_ps(_mm_and_ps(vf0123, vm0123), _mm_andnot_ps(vm0123, _mm_sub_ps(vone, vf0123)));
    vf4567 = _mm_or_ps(_mm_and_ps(vf4567, vm4567), _mm_andnot_ps(vm4567, _mm_sub_ps(vone, vf4567)));

    _mm_storeu_ps(y, vf0123);
    _mm_storeu_ps(y + 4, vf4567);
    y += 8;
  }
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const __m128 vx = _mm_loadu_ps(x);
    x += 4;

    // General structure of the algorithm:
    //           / exp(x) / (1 + exp(x)) if x <= 0
    //   f[x] :=
    //           \ 1 - f[-x] if x >= 0
    //
    // First we compute f[-z] := exp(-z) / (1 + exp(-z)) where z = abs(x),
    // then replace result with 1 - f[-z] if x >= 0.
    const __m128 vz = _mm_and_ps(vx, vnonsign_mask);

    // Compute reduced argument n := round(-z * 64 / log(2)).
    // We do it by adding a large number (magic bias), which cause rounding of the result to an integer, then subtracing
    // the large number back. The first addition is combined with multiplication by log2e into a single FMA instruction.
    // The trick with adding large number is valid only within certain bounds (|z * 64 / log(2)| <= 2**22, i.e.
    // |z| <= 0x1.62E43p+15 = 45426.09375), but that is acceptable, because inputs x outside of [-87.336544, 17.328678]
    // (i.e. z outsize [0, 87.336544]) underflow or saturate sigmoidf(x). We fixup the result  for such inputs at the
    // very end of the algorithm.
    __m128 vn = _mm_add_ps(vmagic_bias, _mm_mul_ps(vz, vminus_log2e_x64));

    // Create a floating-point number s (scale) such that s := 2**(n / 64) for such inputs that sigmoidf(-z) is
    // normalized, i.e. 0 <= z <= 87.33642. As n has 6 fractional bits, we split s == 2**(n / 64) =
    // = 2**e * 2**(n / 64 - e), where e := int(n / 64). We create s in two steps:
    // 1. Fetch 2**(n / 64 - e) = 2**(n % 64) from the table using the 6 low bits of n, as integer. Note that the
    //    fetched values are in the [1.0, 2.0) range, i.e. their floating-point exponent is 0.
    // 2. Adjust fecthed value by addition of e to its floating-point exponent. The result is always a normalized
    //    number, because for 0 <= z <= 87.33642 (inputs for which sigmoidf(-z) is normalized) we have -126 <= e <= 0,
    //    and thus the adjusted exponent is not lower than -126.
    //
    // Extract e from bits 6:14 of n and shift it into bits 23:31 (position of floating-point exponent).
    const __m128i ve = _mm_slli_epi32(_mm_and_si128(_mm_castps_si128(vn), vinv_index_mask), 17);

    // Use bits 0:6 bits of n, as integer, as an index for table lookup of l := 2**(n % 64).
    const __m128i vidx = _mm_slli_epi32(_mm_andnot_si128(vinv_index_mask, _mm_castps_si128(vn)), 2);
    #if XNN_ARCH_X86_64
      const uint64_t vidx_lo = (uint64_t) _mm_cvtsi128_si64(vidx);
      const uint64_t vidx_hi = (uint64_t) _mm_cvtsi128_si64(_mm_unpackhi_epi64(vidx, vidx));
      const __m128i vl0 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx_lo)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx_hi)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx_lo >> 32))));
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx_hi >> 32))));
      const __m128i vl = _mm_unpacklo_epi64(_mm_unpacklo_epi32(vl0, vl1), _mm_unpacklo_epi32(vl2, vl3));
    #else  // !XNN_ARCH_X86_64
      const uint32_t vidx0 = (uint32_t) _mm_cvtsi128_si32(vidx);
      const uint32_t vidx1 = (uint32_t) _mm_extract_epi16(vidx, 2);
      const uint32_t vidx2 = (uint32_t) _mm_extract_epi16(vidx, 4);
      const uint32_t vidx3 = (uint32_t) _mm_extract_epi16(vidx, 6);
      const __m128i vl0 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx0)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx2)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx1)));
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx3)));
      const __m128i vl = _mm_unpacklo_epi64(_mm_unpacklo_epi32(vl0, vl1), _mm_unpacklo_epi32(vl2, vl3));
    #endif  // XNN_ARCH_X86_64
    // Adjust exponent of the value l fetched from the table to get the final s value.
    const __m128 vs = _mm_castsi128_ps(_mm_add_epi32(vl, ve));

    // Subtract the large number back to get the final n := round(-z * 64 / log(2)) as a floating-point number.
    vn = _mm_sub_ps(vn, vmagic_bias);

    // Compute reduced argument t := (z + n * log(2) / 64). Note that -t = -z - n * log(2) / 64.
    // Use Cody-Waite range reduction method (note two constants to represent log(2) / 64) to improve accuracy.
    __m128 vt = _mm_add_ps(vz, _mm_mul_ps(vn, vln2_o64_hi));
    vt = _mm_add_ps(vt, _mm_mul_ps(vn, vln2_o64_lo));

    // Compute degree-2 polynomial approxiatmion for exp(-t) on [-log(2)/128, log(2)/128].
    //   P1(t) = 1 + t * (-1 + t * c2)
    __m128 vp = _mm_mul_ps(vt, vc2);
    vp = _mm_sub_ps(vt, _mm_mul_ps(vp, vt));

    // Reconstruct the exp(-z) value:
    //   f = s * (1 + t * (-1 + t * c2))
    //     = s * (1 - t + t * (t * c2))
    //     = s - s * (t - t * (t * c2))
    //     = s - s * p
    const __m128 vy = _mm_sub_ps(vs, _mm_mul_ps(vs, vp));

    // Reconstruct sigmoid(-z) = exp(-z) / (1.0 + exp(-z))
    __m128 vf = _mm_div_ps(vy, _mm_add_ps(vy, vone));

    // For inputs below denormal cutoff, replace output with +0.0f.
    // Note that for NaN inputs, comparison result is false, and outputs are left unchanged.
    vf = _mm_andnot_ps(_mm_cmplt_ps(vdenorm_cutoff, vz), vf);

    // Reconstruct sigmoid(x) = x < 0 ? sigmoid(-z) : 1.0 - sigmoid(-z)
    const __m128 vm = _mm_castsi128_ps(_mm_cmpgt_epi32(_mm_setzero_si128(), _mm_castps_si128(vx)));
    vf = _mm_or_ps(_mm_and_ps(vm, vf), _mm_andnot_ps(vm, _mm_sub_ps(vone, vf)));

    _mm_storeu_ps(y, vf);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    const __m128 vx = _mm_loadu_ps(x);

    // General structure of the algorithm:
    //           / exp(x) / (1 + exp(x)) if x <= 0
    //   f[x] :=
    //           \ 1 - f[-x] if x >= 0
    //
    // First we compute f[-z] := exp(-z) / (1 + exp(-z)) where z = abs(x),
    // then replace result with 1 - f[-z] if x >= 0.
    const __m128 vz = _mm_and_ps(vx, vnonsign_mask);

    // Compute reduced argument n := round(-z * 64 / log(2)).
    // We do it by adding a large number (magic bias), which cause rounding of the result to an integer, then subtracing
    // the large number back. The first addition is combined with multiplication by log2e into a single FMA instruction.
    // The trick with adding large number is valid only within certain bounds (|z * 64 / log(2)| <= 2**22, i.e.
    // |z| <= 0x1.62E43p+15 = 45426.09375), but that is acceptable, because inputs x outside of [-87.336544, 17.328678]
    // (i.e. z outsize [0, 87.336544]) underflow or saturate sigmoidf(x). We fixup the result  for such inputs at the
    // very end of the algorithm.
    __m128 vn = _mm_add_ps(vmagic_bias, _mm_mul_ps(vz, vminus_log2e_x64));

    // Create a floating-point number s (scale) such that s := 2**(n / 64) for such inputs that sigmoidf(-z) is
    // normalized, i.e. 0 <= z <= 87.33642. As n has 6 fractional bits, we split s == 2**(n / 64) =
    // = 2**e * 2**(n / 64 - e), where e := int(n / 64). We create s in two steps:
    // 1. Fetch 2**(n / 64 - e) = 2**(n % 64) from the table using the 6 low bits of n, as integer. Note that the
    //    fetched values are in the [1.0, 2.0) range, i.e. their floating-point exponent is 0.
    // 2. Adjust fecthed value by addition of e to its floating-point exponent. The result is always a normalized
    //    number, because for 0 <= z <= 87.33642 (inputs for which sigmoidf(-z) is normalized) we have -126 <= e <= 0,
    //    and thus the adjusted exponent is not lower than -126.
    //
    // Extract e from bits 6:14 of n and shift it into bits 23:31 (position of floating-point exponent).
    const __m128i ve = _mm_slli_epi32(_mm_and_si128(_mm_castps_si128(vn), vinv_index_mask), 17);

    // Use bits 0:6 bits of n, as integer, as an index for table lookup of l := 2**(n % 64).
    const __m128i vidx = _mm_slli_epi32(_mm_andnot_si128(vinv_index_mask, _mm_castps_si128(vn)), 2);
    #if XNN_ARCH_X86_64
      const uint64_t vidx_lo = (uint64_t) _mm_cvtsi128_si64(vidx);
      const uint64_t vidx_hi = (uint64_t) _mm_cvtsi128_si64(_mm_unpackhi_epi64(vidx, vidx));
      const __m128i vl0 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx_lo)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) vidx_hi)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx_lo >> 32))));
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + (uint32_t) (vidx_hi >> 32))));
      const __m128i vl = _mm_unpacklo_epi64(_mm_unpacklo_epi32(vl0, vl1), _mm_unpacklo_epi32(vl2, vl3));
    #else  // !XNN_ARCH_X86_64
      const uint32_t vidx0 = (uint32_t) _mm_cvtsi128_si32(vidx);
      const uint32_t vidx1 = (uint32_t) _mm_extract_epi16(vidx, 2);
      const uint32_t vidx2 = (uint32_t) _mm_extract_epi16(vidx, 4);
      const uint32_t vidx3 = (uint32_t) _mm_extract_epi16(vidx, 6);
      const __m128i vl0 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx0)));
      const __m128i vl2 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx2)));
      const __m128i vl1 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx1)));
      const __m128i vl3 = _mm_cvtsi32_si128(*((const int*) ((uintptr_t) xnn_table_exp2_k_over_64 + vidx3)));
      const __m128i vl = _mm_unpacklo_epi64(_mm_unpacklo_epi32(vl0, vl1), _mm_unpacklo_epi32(vl2, vl3));
    #endif  // XNN_ARCH_X86_64

    // Adjust exponent of the value l fetched from the table to get the final s value.
    const __m128 vs = _mm_castsi128_ps(_mm_add_epi32(vl, ve));

    // Subtract the large number back to get the final n := round(-z * 64 / log(2)) as a floating-point number.
    vn = _mm_sub_ps(vn, vmagic_bias);

    // Compute reduced argument t := (z + n * log(2) / 64). Note that -t = -z - n * log(2) / 64.
    // Use Cody-Waite range reduction method (note two constants to represent log(2) / 64) to improve accuracy.
    __m128 vt = _mm_add_ps(vz, _mm_mul_ps(vn, vln2_o64_hi));
    vt = _mm_add_ps(vt, _mm_mul_ps(vn, vln2_o64_lo));

    // Compute degree-2 polynomial approxiatmion for exp(-t) on [-log(2)/128, log(2)/128].
    //   P1(t) = 1 + t * (-1 + t * c2)
    __m128 vp = _mm_mul_ps(vt, vc2);
    vp = _mm_sub_ps(vt, _mm_mul_ps(vp, vt));

    // Reconstruct the exp(-z) value:
    //   f = s * (1 + t * (-1 + t * c2))
    //     = s * (1 - t + t * (t * c2))
    //     = s - s * (t - t * (t * c2))
    //     = s - s * p
    const __m128 vy = _mm_sub_ps(vs, _mm_mul_ps(vs, vp));

    // Reconstruct sigmoid(-z) = exp(-z) / (1.0 + exp(-z))
    __m128 vf = _mm_div_ps(vy, _mm_add_ps(vy, vone));

    // For inputs below denormal cutoff, replace output with +0.0f.
    // Note that for NaN inputs, comparison result is false, and outputs are left unchanged.
    vf = _mm_andnot_ps(_mm_cmplt_ps(vdenorm_cutoff, vz), vf);

    // Reconstruct sigmoid(x) = x < 0 ? sigmoid(-z) : 1.0 - sigmoid(-z)
    const __m128 vm = _mm_castsi128_ps(_mm_cmpgt_epi32(_mm_setzero_si128(), _mm_castps_si128(vx)));
    vf = _mm_or_ps(_mm_and_ps(vm, vf), _mm_andnot_ps(vm, _mm_sub_ps(vone, vf)));

    if (n & (2 * sizeof(float))) {
      _mm_storel_pi((__m64*) y, vf);
      vf = _mm_movehl_ps(vf, vf);
      y += 2;
    }
    if (n & (1 * sizeof(float))) {
      _mm_store_ss(y, vf);
    }
  }
}
