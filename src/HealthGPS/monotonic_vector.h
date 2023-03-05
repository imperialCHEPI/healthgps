#pragma once

#include <vector>
#include <stdexcept>

#include "HealthGPS.Core/forward_type.h"

namespace hgps {

	/// @brief Determine whether a vector data is strict monotonic
	/// @tparam TYPE The vector data type
	/// @param values The vector value
	/// @return true, if the vector data is strict monotonic; otherwise, false
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

	/// @brief Defines a monotonic vector container type
	/// @tparam TYPE The type of the elements 
	template<core::Numerical TYPE>
	class MonotonicVector {
	public:
		/// @brief Element iterator
		using IteratorType = typename std::vector<TYPE>::iterator;
		/// @brief Read-only element iterator
		using ConstIteratorType = typename std::vector<TYPE>::const_iterator;
		
		/// @brief Initialises a new instance of the MonotonicVector class.
		/// @param values The monotonic values
		/// @throws std::invalid_argument if the values are not strict monotonic.
		MonotonicVector(std::vector<TYPE>& values)
			: data_{ values }
		{
			if (!is_strict_monotonic(values)) {
				throw std::invalid_argument("Values must be strict monotonic.");
			}
		}

		/// @brief Gets the number of elements
		/// @return NUmber of elements
		std::size_t size() const noexcept { return data_.size(); }

		/// @brief Gets a specified element with bounds checking
		/// @param index The element index
		/// @return Reference to the requested element.
		const TYPE& at(std::size_t index) const { return data_.at(index); }

		/// @brief Gets a specified element with bounds checking
		/// @param index The element index
		/// @return Reference to the requested element.
		TYPE& operator[](std::size_t index) { return data_.at(index); }

		/// @brief Gets a specified read-only element with bounds checking
		/// @param index The element index
		/// @return Reference to the requested element.
		const TYPE& operator[](std::size_t index) const { return data_.at(index); }

		/// @brief Gets an iterator to the beginning of the elements
		/// @return Iterator to the first element
		IteratorType begin() noexcept { return data_.begin(); }

		/// @brief Gets an iterator to the element following the element of the vector.
		/// @return Iterator to the element following the last element.
		IteratorType end() noexcept { return data_.end(); }

		/// @brief Gets an read-only iterator to the beginning of the elements
		/// @return Iterator to the first element
		ConstIteratorType begin() const noexcept { return data_.cbegin(); }

		/// @brief Gets an read-only iterator to the element following the element of the vector.
		/// @return Iterator to the element following the last element.
		ConstIteratorType end() const noexcept { return data_.cend(); }

		/// @brief Gets an read-only iterator to the beginning of the elements
		/// @return Iterator to the first element
		ConstIteratorType cbegin() const noexcept { return data_.cbegin(); }

		/// @brief Gets an read-only iterator to the element following the element of the vector.
		/// @return Iterator to the element following the last element.
		ConstIteratorType cend() const noexcept { return data_.cend(); }

	private:
		std::vector<TYPE> data_;
	};
}
