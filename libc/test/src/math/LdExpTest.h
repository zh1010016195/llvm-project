//===-- Utility class to test different flavors of ldexp --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TEST_SRC_MATH_LDEXPTEST_H
#define LLVM_LIBC_TEST_SRC_MATH_LDEXPTEST_H

#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/NormalFloat.h"
#include "utils/UnitTest/FPMatcher.h"
#include "utils/UnitTest/Test.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

template <typename T>
class LdExpTestTemplate : public __llvm_libc::testing::Test {
  using FPBits = __llvm_libc::fputil::FPBits<T>;
  using NormalFloat = __llvm_libc::fputil::NormalFloat<T>;
  using UIntType = typename FPBits::UIntType;
  static constexpr UIntType mantissaWidth =
      __llvm_libc::fputil::MantissaWidth<T>::VALUE;
  // A normalized mantissa to be used with tests.
  static constexpr UIntType mantissa = NormalFloat::ONE + 0x1234;

  const T zero = T(__llvm_libc::fputil::FPBits<T>::zero());
  const T neg_zero = T(__llvm_libc::fputil::FPBits<T>::neg_zero());
  const T inf = T(__llvm_libc::fputil::FPBits<T>::inf());
  const T neg_inf = T(__llvm_libc::fputil::FPBits<T>::neg_inf());
  const T nan = T(__llvm_libc::fputil::FPBits<T>::build_nan(1));

public:
  typedef T (*LdExpFunc)(T, int);

  void testSpecialNumbers(LdExpFunc func) {
    int expArray[5] = {-INT_MAX - 1, -10, 0, 10, INT_MAX};
    for (int exp : expArray) {
      ASSERT_FP_EQ(zero, func(zero, exp));
      ASSERT_FP_EQ(neg_zero, func(neg_zero, exp));
      ASSERT_FP_EQ(inf, func(inf, exp));
      ASSERT_FP_EQ(neg_inf, func(neg_inf, exp));
      ASSERT_FP_EQ(nan, func(nan, exp));
    }
  }

  void testPowersOfTwo(LdExpFunc func) {
    int32_t expArray[5] = {1, 2, 3, 4, 5};
    int32_t valArray[6] = {1, 2, 4, 8, 16, 32};
    for (int32_t exp : expArray) {
      for (int32_t val : valArray) {
        ASSERT_FP_EQ(T(val << exp), func(T(val), exp));
        ASSERT_FP_EQ(T(-1 * (val << exp)), func(T(-val), exp));
      }
    }
  }

  void testOverflow(LdExpFunc func) {
    NormalFloat x(FPBits::MAX_EXPONENT - 10, NormalFloat::ONE + 0xF00BA, 0);
    for (int32_t exp = 10; exp < 100; ++exp) {
      ASSERT_FP_EQ(inf, func(T(x), exp));
      ASSERT_FP_EQ(neg_inf, func(-T(x), exp));
    }
  }

  void testUnderflowToZeroOnNormal(LdExpFunc func) {
    // In this test, we pass a normal nubmer to func and expect zero
    // to be returned due to underflow.
    int32_t baseExponent = FPBits::EXPONENT_BIAS + mantissaWidth;
    int32_t expArray[] = {baseExponent + 5, baseExponent + 4, baseExponent + 3,
                          baseExponent + 2, baseExponent + 1};
    T x = NormalFloat(0, mantissa, 0);
    for (int32_t exp : expArray) {
      ASSERT_FP_EQ(func(x, -exp), x > 0 ? zero : neg_zero);
    }
  }

  void testUnderflowToZeroOnSubnormal(LdExpFunc func) {
    // In this test, we pass a normal nubmer to func and expect zero
    // to be returned due to underflow.
    int32_t baseExponent = FPBits::EXPONENT_BIAS + mantissaWidth;
    int32_t expArray[] = {baseExponent + 5, baseExponent + 4, baseExponent + 3,
                          baseExponent + 2, baseExponent + 1};
    T x = NormalFloat(-FPBits::EXPONENT_BIAS, mantissa, 0);
    for (int32_t exp : expArray) {
      ASSERT_FP_EQ(func(x, -exp), x > 0 ? zero : neg_zero);
    }
  }

  void testNormalOperation(LdExpFunc func) {
    T valArray[] = {
        // Normal numbers
        NormalFloat(100, mantissa, 0), NormalFloat(-100, mantissa, 0),
        NormalFloat(100, mantissa, 1), NormalFloat(-100, mantissa, 1),
        // Subnormal numbers
        NormalFloat(-FPBits::EXPONENT_BIAS, mantissa, 0),
        NormalFloat(-FPBits::EXPONENT_BIAS, mantissa, 1)};
    for (int32_t exp = 0; exp <= static_cast<int32_t>(mantissaWidth); ++exp) {
      for (T x : valArray) {
        // We compare the result of ldexp with the result
        // of the native multiplication/division instruction.
        ASSERT_FP_EQ(func(x, exp), x * (UIntType(1) << exp));
        ASSERT_FP_EQ(func(x, -exp), x / (UIntType(1) << exp));
      }
    }

    // Normal which trigger mantissa overflow.
    T x = NormalFloat(-FPBits::EXPONENT_BIAS + 1, 2 * NormalFloat::ONE - 1, 0);
    ASSERT_FP_EQ(func(x, -1), x / 2);
    ASSERT_FP_EQ(func(-x, -1), -x / 2);

    // Start with a normal number high exponent but pass a very low number for
    // exp. The result should be a subnormal number.
    x = NormalFloat(FPBits::EXPONENT_BIAS, NormalFloat::ONE, 0);
    int exp = -FPBits::MAX_EXPONENT - 5;
    T result = func(x, exp);
    FPBits resultBits(result);
    ASSERT_FALSE(resultBits.is_zero());
    // Verify that the result is indeed subnormal.
    ASSERT_EQ(resultBits.get_unbiased_exponent(), uint16_t(0));
    // But if the exp is so less that normalization leads to zero, then
    // the result should be zero.
    result = func(x, -FPBits::MAX_EXPONENT - int(mantissaWidth) - 5);
    ASSERT_TRUE(FPBits(result).is_zero());

    // Start with a subnormal number but pass a very high number for exponent.
    // The result should not be infinity.
    x = NormalFloat(-FPBits::EXPONENT_BIAS + 1, NormalFloat::ONE >> 10, 0);
    exp = FPBits::MAX_EXPONENT + 5;
    ASSERT_FALSE(FPBits(func(x, exp)).is_inf());
    // But if the exp is large enough to oversome than the normalization shift,
    // then it should result in infinity.
    exp = FPBits::MAX_EXPONENT + 15;
    ASSERT_FP_EQ(func(x, exp), inf);
  }
};

#define LIST_LDEXP_TESTS(T, func)                                              \
  using LlvmLibcLdExpTest = LdExpTestTemplate<T>;                              \
  TEST_F(LlvmLibcLdExpTest, SpecialNumbers) { testSpecialNumbers(&func); }     \
  TEST_F(LlvmLibcLdExpTest, PowersOfTwo) { testPowersOfTwo(&func); }           \
  TEST_F(LlvmLibcLdExpTest, OverFlow) { testOverflow(&func); }                 \
  TEST_F(LlvmLibcLdExpTest, UnderflowToZeroOnNormal) {                         \
    testUnderflowToZeroOnNormal(&func);                                        \
  }                                                                            \
  TEST_F(LlvmLibcLdExpTest, UnderflowToZeroOnSubnormal) {                      \
    testUnderflowToZeroOnSubnormal(&func);                                     \
  }                                                                            \
  TEST_F(LlvmLibcLdExpTest, NormalOperation) { testNormalOperation(&func); }

#endif // LLVM_LIBC_TEST_SRC_MATH_LDEXPTEST_H
