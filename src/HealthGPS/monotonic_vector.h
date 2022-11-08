#pragma once

#include <vector>
#include <stdexcept>

#include "HealthGPS.Core/forward_type.h"

namespace hgps {

	template<core::Numerical TYPE>
	bool is_strict_monotonic(const std::vector<TYPE>& values) noexcept {
		auto compare = [](TYPE a, TYPE b) { return a < b ? -1 : (a == b) ? 0 : 1; };

		int previous = 0;
		int offset = 1;
		auto count = values.size();
		if (count > 0) {
			count--;
		}

		for (std::size_t i = 0; i < count; ++i) {
			int current = compare(values[i], values[i + offset]);
			if (current == 0) {
				return false;
			}

			if (current != previous && previous != 0) {
				return false;
			}

			previous = current;
		}

		return true;
	}

	template<core::Numerical TYPE>
	class MonotonicVector {
	public:
		using IteratorType = typename std::vector<TYPE>::iterator;
		using ConstIteratorType = typename std::vector<TYPE>::const_iterator;

		MonotonicVector(std::vector<TYPE>& values)
			: data_{ values }
		{
			if (!is_strict_monotonic(values)) {
				throw std::invalid_argument("Values must be strict monotonic.");
			}
		}

		std::size_t size() const noexcept { return data_.size(); }

		const TYPE& at(std::size_t index) const { return data_.at(index); }

		TYPE& operator[](std::size_t index) { return data_.at(index); }
		const TYPE& operator[](std::size_t index) const { return data_.at(index); }

		IteratorType begin() noexcept { return data_.begin(); }
		IteratorType end() noexcept { return data_.end(); }

		ConstIteratorType begin() const noexcept { return data_.begin(); }
		ConstIteratorType end() const noexcept { return data_.end(); }

	private:
		std::vector<TYPE> data_;
	};
}
