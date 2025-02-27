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
 * Set/get Default kernels for statevector
 */
#pragma once
#include "Error.hpp"
#include "GateOperation.hpp"
#include "IntegerInterval.hpp"
#include "KernelType.hpp"
#include "Threading.hpp"
#include "Util.hpp"

#include <deque>
#include <functional>
#include <unordered_map>
#include <utility>

namespace Pennylane::KernelMap {
///@cond DEV
namespace Internal {

int assignDefaultKernelsForGateOp();
int assignDefaultKernelsForGeneratorOp();
int assignDefaultKernelsForMatrixOp();

template <class Operation> struct AssignKernelForOp;

template <> struct AssignKernelForOp<Gates::GateOperation> {
    static inline const int dummy = assignDefaultKernelsForGateOp();
};
template <> struct AssignKernelForOp<Gates::GeneratorOperation> {
    static inline const int dummy = assignDefaultKernelsForGeneratorOp();
};
template <> struct AssignKernelForOp<Gates::MatrixOperation> {
    static inline const int dummy = assignDefaultKernelsForMatrixOp();
};
} // namespace Internal
///@endcond

///@cond DEV
struct DispatchElement {
    uint32_t priority;
    Util::IntegerInterval<size_t> interval;
    Gates::KernelType kernel;
};

inline bool lower_priority(const DispatchElement &lhs,
                           const DispatchElement &rhs) {
    return lhs.priority < rhs.priority;
}

inline bool higher_priority(const DispatchElement &lhs,
                            const DispatchElement &rhs) {
    return lhs.priority > rhs.priority;
}

/**
 * @brief Maintain dispatch element using a vector decreasingly-ordered by
 * priority.
 */
class PriorityDispatchSet {
  private:
    std::vector<DispatchElement> ordered_vec_;

  public:
    [[nodiscard]] bool
    conflict(uint32_t test_priority,
             const Util::IntegerInterval<size_t> &test_interval) const {
        const auto test_elt = DispatchElement{test_priority, test_interval,
                                              Gates::KernelType::None};
        const auto [b, e] =
            std::equal_range(ordered_vec_.begin(), ordered_vec_.end(), test_elt,
                             higher_priority);
        for (auto iter = b; iter != e; ++iter) {
            if (!is_disjoint(iter->interval, test_interval)) {
                return true;
            }
        }
        return false;
    }

    void insert(const DispatchElement &elt) {
        const auto iter_to_insert = std::upper_bound(
            ordered_vec_.begin(), ordered_vec_.end(), elt, &higher_priority);
        ordered_vec_.insert(iter_to_insert, elt);
    }

    template <typename... Ts> void emplace(Ts &&...args) {
        auto elt = DispatchElement{std::forward<Ts>(args)...};
        const auto iter_to_insert = std::upper_bound(
            ordered_vec_.begin(), ordered_vec_.end(), elt, &higher_priority);
        ordered_vec_.insert(iter_to_insert, elt);
    }

    [[nodiscard]] Gates::KernelType getKernel(size_t num_qubits) const {
        for (const auto &elt : ordered_vec_) {
            if (elt.interval(num_qubits)) {
                return elt.kernel;
            }
        }
        PL_ABORT("Cannot find a kernel for the given number of qubits.");
    }

    void clearPriority(uint32_t remove_priority) {
        const auto begin = std::lower_bound(
            ordered_vec_.begin(), ordered_vec_.end(), remove_priority,
            [](const auto &elt, uint32_t p) { return elt.priority > p; });
        const auto end = std::upper_bound(
            ordered_vec_.begin(), ordered_vec_.end(), remove_priority,
            [](uint32_t p, const auto &elt) { return p > elt.priority; });
        ordered_vec_.erase(begin, end);
    }
};
///@endcond

/**
 * @brief A dummy type used as a tag for a function.
 */
struct AllThreading {};

/**
 * @brief A dummy type used as a tag for a function.
 */
struct AllMemoryModel {};

/**
 * @brief A dummy variable used as a tag for indicating all threading options.
 */
constexpr static AllThreading all_threading{};
/**
 * @brief A dummy variable used as a tag for indicating all memory models.
 */
constexpr static AllMemoryModel all_memory_model{};

/**
 * @brief This class manages all data related to kernel map statevector uses.
 *
 * For a given number of qubit, threading, and memory model, this class
 * returns the best kernels for each gate/generator/matrix operation.
 */
template <class Operation, size_t cache_size = 16> class OperationKernelMap {
  public:
    using EnumDispatchKernalMap =
        std::unordered_map<std::pair<Operation, uint32_t /* dispatch_key */>,
                           PriorityDispatchSet, Util::PairHash>;
    using EnumKernelMap = std::unordered_map<Operation, Gates::KernelType>;

  private:
    EnumDispatchKernalMap kernel_map_;
    mutable std::deque<std::tuple<size_t, uint32_t, EnumKernelMap>> cache_;

    /**
     * @brief Allowed kernels for a given memory model
     */
    const std::unordered_map<CPUMemoryModel, std::vector<Gates::KernelType>>
        allowed_kernels_;

    OperationKernelMap()
        : allowed_kernels_{
              // LCOV_EXCL_START
              {CPUMemoryModel::Unaligned,
               {Gates::KernelType::LM, Gates::KernelType::PI}},
              {CPUMemoryModel::Aligned256,
               {Gates::KernelType::LM, Gates::KernelType::PI}},
              {CPUMemoryModel::Aligned512,
               {Gates::KernelType::LM, Gates::KernelType::PI}},
              // LCOV_EXCL_STOP
          } {}

  public:
    /**
     * @brief Get a singleton instance.
     *
     * @return A singleton instance.
     */
    static auto getInstance() -> OperationKernelMap & {
        static OperationKernelMap instance;

        return instance;
    }

    /**
     * @brief Assign a kernel for a given operation, threading, and memory
     * model.
     *
     * Variable `%priority` set the priority of the given kernel when multiple
     * choices are available. The given `%interval` must be disjoint
     * with all existing intervals with a given priority.
     *
     * @param op Operation to use as a dispatch key
     * @param threading Threading option to use as a dispatch key
     * @param memory_model Memory model to use as a dispatch key
     * @param priority Priority of this assignment
     * @param interval Range of the number of qubits to use this kernel
     * @param kernel Kernel to assign
     */
    void assignKernelForOp(Operation op, Threading threading,
                           CPUMemoryModel memory_model, uint32_t priority,
                           const Util::IntegerInterval<size_t> &interval,
                           Gates::KernelType kernel) {
        if (std::find(allowed_kernels_.at(memory_model).cbegin(),
                      allowed_kernels_.at(memory_model).cend(),
                      kernel) == allowed_kernels_.at(memory_model).cend()) {
            PL_ABORT("The given kernel is not allowed for "
                     "the given memory model.");
        }
        const auto dispatch_key = toDispatchKey(threading, memory_model);
        auto &set = kernel_map_[std::make_pair(op, dispatch_key)];

        PL_ABORT_IF(set.conflict(priority, interval),
                    "The given interval conflicts with existing intervals.");

        // Reset cache
        cache_.clear();

        set.emplace(priority, interval, kernel);
    }

    /**
     * @brief Assign kernel for given operation and memory model for all
     * threading options. The priority of this assignment is 1.
     */
    void assignKernelForOp(Operation op, [[maybe_unused]] AllThreading dummy,
                           CPUMemoryModel memory_model,
                           const Util::IntegerInterval<size_t> &interval,
                           Gates::KernelType kernel) {
        /* Priority for all threading is 1 */
        Util::for_each_enum<Threading>([=, this](Threading threading) {
            assignKernelForOp(op, threading, memory_model, 1, interval, kernel);
        });
    }

    /**
     * @brief Assign kernel for given operation and threading option for all
     * memory models. The priority of this assignment is 2.
     */
    void assignKernelForOp(Operation op, Threading threading,
                           [[maybe_unused]] AllMemoryModel dummy,
                           const Util::IntegerInterval<size_t> &interval,
                           Gates::KernelType kernel) {
        /* Priority for all memory model is 2 */
        Util::for_each_enum<CPUMemoryModel>([=, this](
                                                CPUMemoryModel memory_model) {
            assignKernelForOp(op, threading, memory_model, 2, interval, kernel);
        });
    }

    /**
     * @brief Assign kernel for a given operation for all memory model and all
     * threading options. The priority of this assignment is 0.
     */
    void assignKernelForOp(Operation op, [[maybe_unused]] AllThreading dummy1,
                           [[maybe_unused]] AllMemoryModel dummy2,
                           const Util::IntegerInterval<size_t> &interval,
                           Gates::KernelType kernel) {
        /* Priority is 0 */
        Util::for_each_enum<Threading, CPUMemoryModel>(
            [=, this](Threading threading, CPUMemoryModel memory_model) {
                assignKernelForOp(op, threading, memory_model, 0, interval,
                                  kernel);
            });
    }

    /**
     * @brief Remove an assigned kernel for the given operation, threading,
     * and memory model.
     *
     * @param op Operation
     * @param threading Threading option
     * @param memory_model Memory model
     * @param priority Priority to remove
     */
    void removeKernelForOp(Operation op, Threading threading,
                           CPUMemoryModel memory_model, uint32_t priority) {
        uint32_t dispatch_key = toDispatchKey(threading, memory_model);
        const auto key = std::make_pair(op, dispatch_key);

        const auto iter = kernel_map_.find(key);
        PL_ABORT_IF(iter == kernel_map_.end(),
                    "The given key pair does not exists.");
        (iter->second).clearPriority(priority);

        // Reset cache
        cache_.clear();
    }

    /**
     * @brief Create map contains default kernels for operation
     *
     * @param num_qubits Number of qubits
     * @param threading Threading context
     * @param memory_model Memory model of the underlying data
     * @return A kernel map for given keys
     */
    [[nodiscard]] auto getKernelMap(size_t num_qubits, Threading threading,
                                    CPUMemoryModel memory_model) const
        -> EnumKernelMap {
        // TODO: Add mutex for cache_ when we goto multithread.
        const uint32_t dispatch_key = toDispatchKey(threading, memory_model);

        const auto cache_iter =
            std::find_if(cache_.begin(), cache_.end(), [=](const auto &elt) {
                return (std::get<0>(elt) == num_qubits) &&
                       (std::get<1>(elt) == dispatch_key);
            });
        if (cache_iter == cache_.end()) {
            std::unordered_map<Operation, Gates::KernelType> kernel_for_op;

            Util::for_each_enum<Operation>([&](Operation op) {
                const auto key = std::make_pair(op, dispatch_key);
                const auto &set = kernel_map_.at(key);
                kernel_for_op.emplace(op, set.getKernel(num_qubits));
            });
            if (cache_.size() == cache_size) {
                cache_.pop_back();
            }
            cache_.emplace_front(num_qubits, dispatch_key, kernel_for_op);
            return kernel_for_op;
        }
        return std::get<2>(*cache_iter);
    }
};
} // namespace Pennylane::KernelMap
