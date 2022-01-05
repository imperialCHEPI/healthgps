#pragma once
#include "forward_type.h"

#include <memory>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <fmt/format.h>

namespace hgps {
	namespace core {

		template<Numerical TYPE>
		class Array2D
		{
		public:
			Array2D() = default;
			Array2D(const size_t nrows, const size_t ncols) 
				: rows_{ nrows }, columns_{ ncols }, data_(nrows * ncols)
			{
				if (nrows <= 0 || ncols <= 0) {
					throw std::invalid_argument("Invalid array constructor with 0 size");
				}

				data_.shrink_to_fit();
			}

			Array2D(const size_t nrows, const size_t ncols, TYPE value)
				: Array2D(nrows, ncols) {
				fill(value);
			}

			Array2D(const size_t nrows, const size_t ncols, std::vector<TYPE>& values) 
				: Array2D(nrows, ncols) {

				if (values.size() != size()) {
					throw std::invalid_argument(
						fmt::format("Array size and values size mismatch: {} vs. given {}.", size(), values.size()));
				}

				std::copy(values.begin(), values.end(), data_.begin());
			}

			size_t size() const noexcept { return rows_ * columns_; }

			size_t rows() const noexcept { return rows_; }

			size_t columns() const noexcept { return columns_; }

			TYPE& operator()(size_t row, size_t column) {
				check_boundaries(row, column);
				return data_[row * columns_ + column]; 
			}

			const TYPE& operator()(size_t row, size_t column) const {
				check_boundaries(row, column);
				return data_[row * columns_ + column];
			}

			void fill(TYPE value) { std::fill(data_.begin(), data_.end(), value); }

			void clear() { fill(TYPE{}); }

			std::vector<TYPE> to_vector() {
				return std::vector<TYPE>(data_);
			}

			std::string to_string() noexcept {
				std::stringstream ss;
				ss << "Array: " << rows_ << "x" << columns_ << "\n";
				for (size_t i = 0; i < rows_; i++) {
					ss << "{";
					for (size_t j = 0; j < columns_; j++) {
						ss << operator()(i, j);
						if ((j + 1) != columns_) ss << ", ";
					}

					ss << "}\n";
				}

				return ss.str();
			}

		private:
			size_t rows_{};
			size_t columns_{};
			std::vector<TYPE> data_;

			void check_boundaries(size_t row, size_t column) const {
				if ((row < 0) || (row >= rows_)) {
					throw std::out_of_range(fmt::format("Row {} is out of array bounds.", row));
				}
				
				if ((column < 0) || (column >= columns_)) {
					throw std::out_of_range(fmt::format("Column {} is out of array bounds.", column));
				}
			}
		};

		using FloatArray2D = Array2D<float>;
		using DoubleArray2D = Array2D<double>;
		using IntegerArray2D = Array2D<int>;
	}
}