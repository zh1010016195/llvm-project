//===-- Utility class to test different flavors of fma --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TEST_SRC_MATH_FMATEST_H
#define LLVM_LIBC_TEST_SRC_MATH_FMATEST_H

#include "src/__support/FPUtil/FPBits.h"
#include "utils/MPFRWrapper/MPFRUtils.h"
#include "utils/UnitTest/FPMatcher.h"
#include "utils/UnitTest/Test.h"
#include "utils/testutils/RandUtils.h"

namespace mpfr = __llvm_libc::testing::mpfr;

template <typename T>
class FmaTestTemplate : public __llvm_libc::testing::Test {
private:
  using Func = T (*)(T, T, T);
  using FPBits = __llvm_libc::fputil::FPBits<T>;
  using UIntType = typename FPBits::UIntType;
  const T nan = T(__llvm_libc::fputil::FPBits<T>::build_nan(1));
  const T inf = T(__llvm_libc::fputil::FPBits<T>::inf());
  const T neg_inf = T(__llvm_libc::fputil::FPBits<T>::neg_inf());
  const T zero = T(__llvm_libc::fputil::FPBits<T>::zero());
  const T neg_zero = T(__llvm_libc::fputil::FPBits<T>::neg_zero());

  UIntType getRandomBitPattern() {
    UIntType bits{0};
    for (UIntType i = 0; i < sizeof(UIntType) / 2; ++i) {
      bits =
          (bits << 2) + static_cast<uint16_t>(__llvm_libc::testutils::rand());
    }
    return bits;
  }

public:
  void testSpecialNumbers(Func func) {
    EXPECT_FP_EQ(func(zero, zero, zero), zero);
    EXPECT_FP_EQ(func(zero, neg_zero, neg_zero), neg_zero);
    EXPECT_FP_EQ(func(inf, inf, zero), inf);
    EXPECT_FP_EQ(func(neg_inf, inf, neg_inf), neg_inf);
    EXPECT_FP_EQ(func(inf, zero, zero), nan);
    EXPECT_FP_EQ(func(inf, neg_inf, inf), nan);
    EXPECT_FP_EQ(func(nan, zero, inf), nan);
    EXPECT_FP_EQ(func(inf, neg_inf, nan), nan);

    // Test underflow rounding up.
    EXPECT_FP_EQ(func(T(0.5), T(FPBits(FPBits::MIN_SUBNORMAL)),
                      T(FPBits(FPBits::MIN_SUBNORMAL))),
                 T(FPBits(UIntType(2))));
    // Test underflow rounding down.
    T v = T(FPBits(FPBits::MIN_NORMAL + UIntType(1)));
    EXPECT_FP_EQ(func(T(1) / T(FPBits::MIN_NORMAL << 1), v,
                      T(FPBits(FPBits::MIN_NORMAL))),
                 v);
    // Test overflow.
    T z = T(FPBits(FPBits::MAX_NORMAL));
    EXPECT_FP_EQ(func(T(1.75), z, -z), T(0.75) * z);
  }

  void testSubnormalRange(Func func) {
    constexpr UIntType count = 1000001;
    constexpr UIntType step =
        (FPBits::MAX_SUBNORMAL - FPBits::MIN_SUBNORMAL) / count;
    for (UIntType v = FPBits::MIN_SUBNORMAL, w = FPBits::MAX_SUBNORMAL;
         v <= FPBits::MAX_SUBNORMAL && w >= FPBits::MIN_SUBNORMAL;
         v += step, w -= step) {
      T x = T(FPBits(getRandomBitPattern())), y = T(FPBits(v)),
        z = T(FPBits(w));
      T result = func(x, y, z);
      mpfr::TernaryInput<T> input{x, y, z};
      ASSERT_MPFR_MATCH(mpfr::Operation::Fma, input, result, 0.5);
    }
  }

  void testNormalRange(Func func) {
    constexpr UIntType count = 1000001;
    constexpr UIntType step = (FPBits::MAX_NORMAL - FPBits::MIN_NORMAL) / count;
    for (UIntType v = FPBits::MIN_NORMAL, w = FPBits::MAX_NORMAL;
         v <= FPBits::MAX_NORMAL && w >= FPBits::MIN_NORMAL;
         v += step, w -= step) {
      T x = T(FPBits(v)), y = T(FPBits(w)),
        z = T(FPBits(getRandomBitPattern()));
      T result = func(x, y, z);
      mpfr::TernaryInput<T> input{x, y, z};
      ASSERT_MPFR_MATCH(mpfr::Operation::Fma, input, result, 0.5);
    }
  }
};

#endif // LLVM_LIBC_TEST_SRC_MATH_FMATEST_H
