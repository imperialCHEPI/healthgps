#pragma once

#include <thread>
#include <future>
#include <mutex>
#include <execution>
#include <numeric>

namespace hgps {
	namespace core {
		
		inline constexpr auto execution_policy = std::execution::par;

		template<class F, class... Ts>
		auto run_async(F&& action, Ts&&... params)
		{
			return std::async(std::launch::async,
				std::forward<F>(action),
				std::forward<Ts>(params)...);
		};

		template <class Policy, class Index, class UnaryFunction>
		auto parallel_for(Policy&& policy, Index first, Index last, UnaryFunction func) {
			auto range = std::vector<size_t>(last - first + 1);
			std::iota(range.begin(), range.end(), first);
			std::for_each(policy, range.begin(), range.end(), std::move(func));
		}

		template <class Policy, class T, class UnaryPredicate>
		auto find_index_of_all(Policy&& policy, const T& data , UnaryPredicate pred) {
			auto index_mutex = std::mutex{};
			auto result = std::vector<size_t>{};
			parallel_for(policy, size_t{ 0 }, data.size() - 1, [&](size_t index)
				{
					if (pred(data[index])) {
						auto lock = std::unique_lock{ index_mutex };
						result.emplace_back(index);
					}
				});

			result.shrink_to_fit();
			return result;
		}
	}
}