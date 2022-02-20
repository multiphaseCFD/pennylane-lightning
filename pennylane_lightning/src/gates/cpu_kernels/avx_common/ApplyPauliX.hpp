// Copyright 2022 Xanadu Quantum Technologies Inc.

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
 * Defines PauliX gate
 */
#pragma once
#include "AVXUtil.hpp"
#include "BitUtil.hpp"
#include "Util.hpp"

#include <immintrin.h>

#include <complex>

namespace Pennylane::Gates::AVX {

template<typename PrecisionT, template<typename> class AVXConcept>
struct ApplyPauliX {
    using PrecisionAVXConcept = AVXConcept<PrecisionT>;
    using RealProd = typename AVXConcept<PrecisionT>::RealProd;
    using ImagProd = typename AVXConcept<PrecisionT>::ImagProd;

    template <size_t rev_wire>
    static void applyInternal(std::complex<PrecisionT> *arr,
                              const size_t num_qubits) {
        for (size_t k = 0; k < (1U << num_qubits);
             k += PrecisionAVXConcept::step_for_complex_precision) {
            const auto v = PrecisionAVXConcept::load(arr + k);
            PrecisionAVXConcept::store(arr + k,
                    PrecisionAVXConcept::template internalSwap<rev_wire>(v));
        }
    }

    static void applyExternal(std::complex<PrecisionT> *arr,
                              const size_t num_qubits,
                              const size_t rev_wire) {
        const size_t rev_wire_shift = (static_cast<size_t>(1U) << rev_wire);
        const size_t wire_parity = fillTrailingOnes(rev_wire);
        const size_t wire_parity_inv = fillLeadingOnes(rev_wire + 1);
        for (size_t k = 0; k < exp2(num_qubits - 1);
             k += PrecisionAVXConcept::step_for_complex_precision) {
            const size_t i0 = ((k << 1U) & wire_parity_inv) | (wire_parity & k);
            const size_t i1 = i0 | rev_wire_shift;

            const auto v0 = PrecisionAVXConcept::load(arr + i0);
            const auto v1 = PrecisionAVXConcept::load(arr + i1);
            PrecisionAVXConcept::store(arr + i0, v1);
            PrecisionAVXConcept::store(arr + i1, v0);
        }
    }
};
} // namespace Pennylane::Gates::AVX
