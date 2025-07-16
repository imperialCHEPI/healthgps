#pragma once

#include <future>
#include <mutex>
#include <numeric>
#include <thread>

#include <oneapi/tbb/parallel_for_each.h>

namespace hgps::core {

/// @brief Run a given function asynchronous
/// @tparam F Function type
/// @tparam ...Ts Function parameters type
/// @param action The action to run
/// @param ...params The action parameters
/// @return The std::future referring to the function call.
template <class F, class... Ts> auto run_async(F &&action, Ts &&...params) {
    return std::async(std::launch::async, std::forward<F>(action), std::forward<Ts>(params)...);
};

/// @brief Parallel for each over an indexed accessed containers
/// @tparam Index Iterator type
/// @tparam UnaryFunction Function type
/// @param first The first element iterator
/// @param last The last element iterator
/// @param func The function object to apply
/// @return none
template <class Index, class UnaryFunction>
auto parallel_for(Index first, Index last, UnaryFunction func) {
    auto range = std::vector<size_t>(last - first + 1);
    std::iota(range.begin(), range.end(), first);
    tbb::parallel_for_each(range.begin(), range.end(), std::move(func));
}

/// @brief Finds index of all occurrences in a container
/// @tparam T Container type
/// @tparam UnaryPredicate Predicate type
/// @param data The container to search
/// @param pred The predicated object to apply
/// @return A std::vector<size_t> with zero-based index of all occurrences.
template <class T, class UnaryPredicate>
auto find_index_of_all(const T &data, UnaryPredicate pred) {
    auto index_mutex = std::mutex{};
    auto result = std::vector<size_t>{};
    parallel_for(size_t{0}, data.size() - 1, [&](size_t index) {
        if (pred(data[index])) {
            auto lock = std::unique_lock{index_mutex};
            result.emplace_back(index);
        }
    });

    result.shrink_to_fit();
    return result;
}

} // namespace hgps::core
