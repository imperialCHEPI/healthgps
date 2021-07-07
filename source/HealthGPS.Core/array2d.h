#pragma once
#include <format>
#include <memory>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "forward_type.h"

namespace hgps {
	namespace core {

		template<Numerical TYPE>
		class Array2D
		{
		public:
			Array2D() = default;
			Array2D(const size_t nrows, const size_t ncols) 
				: rows_{ nrows }, columns_{ ncols }
			{
				if (nrows <= 0 || ncols <= 0) {
					throw std::invalid_argument("Invalid array constructor with 0 size");
				}

				data_ = std::make_shared<TYPE[]>(rows_ * columns_);
			}

			Array2D(const size_t nrows, const size_t ncols, TYPE value)
				: Array2D(nrows, ncols) {
				fill(value);
			}

			Array2D(const size_t nrows, const size_t ncols, std::vector<TYPE>& values) 
				: Array2D(nrows, ncols) {

				if (values.size() > size()) {
					throw std::invalid_argument(
						std::format("Array size and values size mismatch: {} vs. given {}.", size(), values.size()));
				}

				std::copy(values.begin(), values.end(), data_.get());
			}

			size_t size() { return rows_ * columns_; }

			size_t rows() { return rows_; }

			size_t columns() { return columns_; }

			TYPE& operator()(size_t row, size_t column) {
				check_boundaries(row, column);
				return data_[row * columns_ + column]; 
			}

			TYPE operator()(size_t row, size_t column) const {
				check_boundaries(row, column);
				return data_[row * columns_ + column];
			}

			void fill(TYPE value) { std::fill_n(data_.get(), size(), value); }

			void clear() { fill(TYPE{}); }

			std::vector<TYPE> to_vector() {
				return std::vector<TYPE>(data_.get(), data_.get()+size());
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
			std::shared_ptr<TYPE[]> data_;

			void check_boundaries(size_t row, size_t column) {
				if ((row < 0) || (row >= rows_)) {
					throw std::invalid_argument(std::format("Row {} is out of array bounds.", row));
				}
				
				if ((column < 0) || (column >= columns_)) {
					throw std::invalid_argument(std::format("Column {} is out of array bounds.", column));
				}
			}
		};

		using FloatArray2D = Array2D<float>;
		using DoubleArray2D = Array2D<double>;
		using IntegerArray2D = Array2D<int>;
	}
}