// Copyright 2021 Xanadu Quantum Technologies Inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/**
 * @file
 * Defines kernel functions with less memory (and fast)
 */
#pragma once

#include "BitUtil.hpp"
#include "Error.hpp"
#include "GateImplementationsLM.hpp"
#include "GateOperation.hpp"
#include "Gates.hpp"
#include "KernelType.hpp"
#include "LinearAlgebra.hpp"
#include "Macros.hpp"

#include <immintrin.h>

#include <complex>
#include <vector>

namespace Pennylane::Gates {

namespace Internal {

template <class PrecisionT> struct AVX512Intrinsic;

template <> struct AVX512Intrinsic<float> { using Type = __m512; };

template <> struct AVX512Intrinsic<double> { using Type = __m512d; };

template <class PrecisionT>
using AVX512IntrinsicType = typename AVX512Intrinsic<PrecisionT>::Type;

template <class PrecisionT, size_t rev_wire>
auto permuteInternal(AVX512IntrinsicType<PrecisionT> v)
    -> AVX512IntrinsicType<PrecisionT> {
    // Permute internal data of v after grouping two(complex number)
    if constexpr (std::is_same_v<PrecisionT, float>) {
        if constexpr (rev_wire == 0) {
            return _mm512_permute_ps(v, 0B01001110);
        }
        if constexpr (rev_wire == 1) {
            const static auto shuffle_idx = _mm512_set_epi32(
                11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4);
            return _mm512_permutexvar_ps(shuffle_idx, v);
        }
        if constexpr (rev_wire == 2) {
            const static auto shuffle_idx = _mm512_set_epi32(
                7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
            return _mm512_permutexvar_ps(shuffle_idx, v);
        }
    }
    if constexpr (std::is_same_v<PrecisionT, double>) {
        if constexpr (rev_wire == 0) {
            const static auto shuffle_idx =
                _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);
            return _mm512_permutexvar_pd(shuffle_idx, v);
        }
        if constexpr (rev_wire == 1) {
            const static auto shuffle_idx =
                _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4);
            return _mm512_permutexvar_pd(shuffle_idx, v);
        }
    }
}

constexpr uint8_t parity(size_t n, size_t rev_wire) {
    return static_cast<uint8_t>((n >> rev_wire) & 1U);
}
constexpr uint8_t parity(size_t n, size_t rev_wire0, size_t rev_wire1) {
    return static_cast<uint8_t>((n >> rev_wire0) & 1U) ^
           static_cast<uint8_t>((n >> rev_wire1) & 1U);
}

inline __m512 parityS(size_t n, size_t rev_wire) {
    return _mm512_setr_ps(parity(n + 0, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 0, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 1, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 1, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 2, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 2, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 3, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 3, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 4, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 4, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 5, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 5, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 6, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 6, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 7, rev_wire) ? -1.0F : 1.0F,
                          parity(n + 7, rev_wire) ? -1.0F : 1.0F);
}
inline __m512 parityS(size_t n, size_t rev_wire0, size_t rev_wire1) {
    return _mm512_setr_ps(parity(n + 0, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 0, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 1, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 1, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 2, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 2, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 3, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 3, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 4, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 4, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 5, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 5, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 6, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 6, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 7, rev_wire0, rev_wire1) ? -1.0F : 1.0F,
                          parity(n + 7, rev_wire0, rev_wire1) ? -1.0F : 1.0F);
}

inline __m512d parityD(size_t n, size_t rev_wire) {
    return _mm512_setr_pd(parity(n + 0, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 0, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 1, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 1, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 2, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 2, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 3, rev_wire) ? -1.0L : 1.0L,
                          parity(n + 3, rev_wire) ? -1.0L : 1.0L);
}

inline __m512d parityD(size_t n, size_t rev_wire0, size_t rev_wire1) {

    const auto indices = _mm512_setr_epi64(n + 0, n + 0, n + 1, n + 1, n + 2,
                                           n + 2, n + 3, n + 3);
    const auto ones = _mm512_set1_epi64(1U);
    auto parities = _mm512_xor_epi64(_mm512_srli_epi64(indices, rev_wire0),
                                     _mm512_srli_epi64(indices, rev_wire1));
    parities = _mm512_and_epi64(parities, ones);
    if constexpr (use_avx512dq) {
        parities = _mm512_sub_epi64(_mm512_set1_epi64(1U),
                                    _mm512_slli_epi64(parities, 1));
        return _mm512_cvtepi64_pd(parities);
    }

    __m256i parities_32 = _mm512_cvtepi64_epi32(parities);
    parities_32 = _mm256_sub_epi32(_mm256_set1_epi32(1U),
                                   _mm256_slli_epi32(parities_32, 1));
    return _mm512_cvtepi32_pd(parities_32);
}

/**
 * @brief
 *
 * @val is just a sequence of values for a complex value
 */
inline __m512d productImagD(__m512d val, __m512d imag_val) {
    alignas(64) constexpr static double imag_factor[8] = {
        -1.0l, 1.0l, -1.0l, 1.0l, -1.0l, 1.0l, -1.0l, 1.0l,
    };
    __m512d prod_shuffled =
        _mm512_permutex_pd(_mm512_mul_pd(val, imag_val), 0B10110001);
    return _mm512_mul_pd(prod_shuffled, _mm512_load_pd(&imag_factor));
}

inline __m512 productImagS(__m512 val, __m512 imag_val) {
    alignas(64) constexpr static float imag_factor[16] = {
        -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f,

    };
    __m512 prod_shuffled =
        _mm512_permute_ps(_mm512_mul_ps(val, imag_val), 0B10110001);
    return _mm512_mul_ps(prod_shuffled, _mm512_load_ps(&imag_factor));
}
} // namespace Internal

class GateImplementationsAVX512 {
  private:
    /* Alias utility functions */
    static constexpr auto fillLeadingOnes = Util::fillLeadingOnes;
    static constexpr auto fillTrailingOnes = Util::fillTrailingOnes;

  public:
    constexpr static KernelType kernel_id = KernelType::AVX512;
    constexpr static std::string_view name = "AVX512";
    constexpr static uint32_t data_alignment_in_bytes = 64;

    constexpr static std::array implemented_gates = {
        GateOperation::PauliX,
        GateOperation::RZ,
        GateOperation::IsingZZ,
    };

    constexpr static std::array<GeneratorOperation, 0> implemented_generators =
        {};

  private:
    template <size_t rev_wire>
    static void applyPauliX_float_internal(std::complex<float> *arr,
                                           const size_t num_qubits) {
        constexpr static auto step =
            data_alignment_in_bytes / sizeof(float) / 2;
        for (size_t k = 0; k < (1U << num_qubits); k += step) {
            const __m512 v = _mm512_load_ps(arr + k);
            _mm512_store_ps(arr + k,
                            Internal::permuteInternal<float, rev_wire>(v));
        }
    }
    static void applyPauliX_float_external(std::complex<float> *arr,
                                           const size_t num_qubits,
                                           const size_t rev_wire) {
        const size_t rev_wire_shift = (static_cast<size_t>(1U) << rev_wire);
        const size_t wire_parity = fillTrailingOnes(rev_wire);
        const size_t wire_parity_inv = fillLeadingOnes(rev_wire + 1);
        constexpr static auto step =
            data_alignment_in_bytes / sizeof(float) / 2;
        for (size_t k = 0; k < Util::exp2(num_qubits - 1); k += step) {
            const size_t i0 = ((k << 1U) & wire_parity_inv) | (wire_parity & k);
            const size_t i1 = i0 | rev_wire_shift;

            const __m512 v0 = _mm512_load_ps(arr + i0);
            const __m512 v1 = _mm512_load_ps(arr + i1);
            _mm512_store_ps(arr + i0, v1);
            _mm512_store_ps(arr + i1, v0);
        }
    }

    template <size_t rev_wire>
    static void applyPauliX_double_internal(std::complex<double> *arr,
                                            const size_t num_qubits) {
        constexpr static auto step =
            data_alignment_in_bytes / sizeof(double) / 2;
        for (size_t k = 0; k < (1U << num_qubits); k += step) {
            const __m512d v = _mm512_load_pd(arr + k);
            _mm512_store_pd(arr + k,
                            Internal::permuteInternal<double, rev_wire>(v));
        }
    }
    static void applyPauliX_double_external(std::complex<double> *arr,
                                            const size_t num_qubits,
                                            const size_t rev_wire) {
        const size_t rev_wire_shift = (static_cast<size_t>(1U) << rev_wire);
        const size_t wire_parity = fillTrailingOnes(rev_wire);
        const size_t wire_parity_inv = fillLeadingOnes(rev_wire + 1);
        constexpr static auto step =
            data_alignment_in_bytes / sizeof(double) / 2;
        for (size_t k = 0; k < Util::exp2(num_qubits - 1); k += step) {
            const size_t i0 = ((k << 1U) & wire_parity_inv) | (wire_parity & k);
            const size_t i1 = i0 | rev_wire_shift;

            const __m512d v0 = _mm512_load_pd(arr + i0);
            const __m512d v1 = _mm512_load_pd(arr + i1);
            _mm512_store_pd(arr + i0, v1);
            _mm512_store_pd(arr + i1, v0);
        }
    }

  public:
    template <class PrecisionT>
    static void applyPauliX(std::complex<PrecisionT> *arr,
                            const size_t num_qubits,
                            const std::vector<size_t> &wires,
                            [[maybe_unused]] bool inverse) {
        if constexpr (std::is_same_v<PrecisionT, float>) {
            if (num_qubits < 3) {
                GateImplementationsLM::applyPauliX(arr, num_qubits, wires,
                                                   inverse);
                return;
            }
            const size_t rev_wire = num_qubits - wires[0] - 1;

            switch (rev_wire) {
            case 0:
                applyPauliX_float_internal<0>(arr, num_qubits);
                return;
            case 1:
                applyPauliX_float_internal<1>(arr, num_qubits);
                return;
            case 2:
                applyPauliX_float_internal<2>(arr, num_qubits);
                return;
            default:
                applyPauliX_float_external(arr, num_qubits, rev_wire);
                return;
            }
        } else if (std::is_same_v<PrecisionT, double>) {
            if (num_qubits < 2) {
                GateImplementationsLM::applyPauliX(arr, num_qubits, wires,
                                                   inverse);
                return;
            }
            const size_t rev_wire = num_qubits - wires[0] - 1;

            switch (rev_wire) {
            case 0:
                applyPauliX_double_internal<0>(arr, num_qubits);
                return;
            case 1:
                applyPauliX_double_internal<1>(arr, num_qubits);
                return;
            default:
                applyPauliX_double_external(arr, num_qubits, rev_wire);
                return;
            }
        } else {
            static_assert(std::is_same_v<PrecisionT, float> ||
                          std::is_same_v<PrecisionT, double>);
        }
    }
    /* Version iterate over all indices */
    template <class PrecisionT, class ParamT = PrecisionT>
    static void applyRZ(std::complex<PrecisionT> *arr, const size_t num_qubits,
                        const std::vector<size_t> &wires,
                        [[maybe_unused]] bool inverse, ParamT angle) {
        assert(wires.size() == 1);

        if constexpr (std::is_same_v<PrecisionT, float>) {
            if (num_qubits < 3) {
                GateImplementationsLM::applyRZ(arr, num_qubits, wires, inverse,
                                               angle);
            } else {
                const size_t rev_wire = num_qubits - wires[0] - 1;

                __m512 real_cos_factor = _mm512_set1_ps(std::cos(angle / 2));
                const float isin =
                    inverse ? std::sin(angle / 2) : -std::sin(angle / 2);
                __m512 imag_sin_factor = _mm512_set_ps(
                    -isin, isin, -isin, isin, -isin, isin, -isin, isin, -isin,
                    isin, -isin, isin, -isin, isin, -isin, isin);
                for (size_t n = 0; n < (1U << num_qubits); n += 8) {
                    __m512 coeffs = _mm512_load_ps(arr + n);
                    __m512 prod_cos = _mm512_mul_ps(real_cos_factor, coeffs);

                    __m512 imag_sin_parity = _mm512_mul_ps(
                        imag_sin_factor, Internal::parityS(n, rev_wire));
                    __m512 prod_sin = _mm512_mul_ps(coeffs, imag_sin_parity);

                    __m512 prod = _mm512_add_ps(
                        prod_cos, _mm512_permute_ps(prod_sin, 0B10110001));
                    _mm512_store_ps(arr + n, prod);
                }
            }
        } else if (std::is_same_v<PrecisionT, double>) {
            if (num_qubits < 2) {
                GateImplementationsLM::applyRZ(arr, num_qubits, wires, inverse,
                                               angle);
            } else {
                const size_t rev_wire = num_qubits - wires[0] - 1;
                __m512d real_cos_factor = _mm512_set1_pd(std::cos(angle / 2));

                const double isin =
                    inverse ? std::sin(angle / 2) : -std::sin(angle / 2);
                __m512d imag_sin_factor = _mm512_set_pd(
                    -isin, isin, -isin, isin, -isin, isin, -isin, isin);
                for (size_t n = 0; n < (1U << num_qubits); n += 4) {
                    __m512d coeffs = _mm512_load_pd(arr + n);
                    __m512d prod_cos = _mm512_mul_pd(real_cos_factor, coeffs);

                    __m512d imag_sin_parity = _mm512_mul_pd(
                        imag_sin_factor, Internal::parityD(n, rev_wire));
                    __m512d prod_sin = _mm512_mul_pd(coeffs, imag_sin_parity);

                    __m512d prod = _mm512_add_pd(
                        prod_cos, _mm512_permutex_pd(prod_sin, 0B10110001));
                    _mm512_store_pd(arr + n, prod);
                }
            }
        } else {
            static_assert(std::is_same_v<PrecisionT, float> ||
                              std::is_same_v<PrecisionT, double>,
                          "Only float or double is supported.");
        }
    }
    /* Version iterate over all indices */
    template <class PrecisionT, class ParamT = PrecisionT>
    static void applyIsingZZ(std::complex<PrecisionT> *arr,
                             const size_t num_qubits,
                             const std::vector<size_t> &wires,
                             [[maybe_unused]] bool inverse, ParamT angle) {
        assert(wires.size() == 2);

        if constexpr (std::is_same_v<PrecisionT, float>) {
            if (num_qubits < 3) {
                GateImplementationsLM::applyIsingZZ(arr, num_qubits, wires,
                                                    inverse, angle);
            } else {
                const size_t rev_wire0 = num_qubits - wires[0] - 1;
                const size_t rev_wire1 = num_qubits - wires[1] - 1;
                __m512 real_cos_factor = _mm512_set1_ps(std::cos(angle / 2));
                const float isin =
                    inverse ? std::sin(angle / 2) : -std::sin(angle / 2);
                __m512 imag_sin_factor = _mm512_set_ps(
                    -isin, isin, -isin, isin, -isin, isin, -isin, isin, -isin,
                    isin, -isin, isin, -isin, isin, -isin, isin);
                for (size_t n = 0; n < (1U << num_qubits); n += 8) {
                    __m512 coeffs = _mm512_load_ps(arr + n);
                    __m512 prod_cos = _mm512_mul_ps(real_cos_factor, coeffs);

                    __m512 imag_sin_parity = _mm512_mul_ps(
                        imag_sin_factor,
                        Internal::parityS(n, rev_wire0, rev_wire1));
                    __m512 prod_sin = _mm512_mul_ps(coeffs, imag_sin_parity);

                    __m512 prod = _mm512_add_ps(
                        prod_cos, _mm512_permute_ps(prod_sin, 0B10110001));
                    _mm512_store_ps(arr + n, prod);
                }
            }
        } else if (std::is_same_v<PrecisionT, double>) {
            if (num_qubits < 2) {
                GateImplementationsLM::applyIsingZZ(arr, num_qubits, wires,
                                                    inverse, angle);
            } else {
                const size_t rev_wire0 = num_qubits - wires[0] - 1;
                const size_t rev_wire1 = num_qubits - wires[1] - 1;
                __m512d real_cos_factor = _mm512_set1_pd(std::cos(angle / 2));

                const double isin =
                    inverse ? std::sin(angle / 2) : -std::sin(angle / 2);
                __m512d imag_sin_factor = _mm512_set_pd(
                    -isin, isin, -isin, isin, -isin, isin, -isin, isin);
                for (size_t n = 0; n < (1U << num_qubits); n += 4) {
                    __m512d coeffs = _mm512_load_pd(arr + n);
                    __m512d prod_cos = _mm512_mul_pd(real_cos_factor, coeffs);

                    __m512d imag_sin_parity = _mm512_mul_pd(
                        imag_sin_factor,
                        Internal::parityD(n, rev_wire0, rev_wire1));
                    __m512d prod_sin = _mm512_mul_pd(coeffs, imag_sin_parity);

                    __m512d prod = _mm512_add_pd(
                        prod_cos, _mm512_permutex_pd(prod_sin, 0B10110001));
                    _mm512_store_pd(arr + n, prod);
                }
            }
        } else {
            static_assert(std::is_same_v<PrecisionT, float> ||
                              std::is_same_v<PrecisionT, double>,
                          "Only float or double is supported.");
        }
    }
};
} // namespace Pennylane::Gates
