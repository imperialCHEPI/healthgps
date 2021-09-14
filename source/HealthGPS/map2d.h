#pragma once
#include <map>

namespace hgps {
	template<class TROW, class TCOL, class TCell>
	class Map2d
	{
	public:
		using IteratorType = std::map<TROW, std::map<TCOL, TCell>>::iterator;
		using ConstIteratorType = std::map<TROW, std::map<TCOL, TCell>>::const_iterator;

		Map2d() = default;
		Map2d(std::map<TROW, std::map<TCOL, TCell>>&& data) noexcept
			: table_{ data } {}

		bool empty() const noexcept {
			return table_.empty();
		}

		std::size_t size() const noexcept {
			return rows() * columns();
		}

		std::size_t rows() const noexcept {
			return table_.size();
		}

		std::size_t columns() const noexcept {
			if (table_.size() > 0) {

				// Assuming that inner map has same size.
				return table_.begin()->second.size();
			}

			return 0;
		}

		bool contains(const TROW& row_key) const {
			return table_.contains(row_key);
		}

		bool contains(const TROW& row_key, const TCOL& col_key) const {
			if (table_.contains(row_key)) {
				return table_.at(row_key).contains(col_key);
			}

			return false;
		}

		std::map<TCOL, TCell>& row(const TROW& row_key) {
			return table_.at(row_key);
		}

		const std::map<TCOL, TCell>& row(const TROW& row_key) const {
			return table_.at(row_key);
		}

		TCell& at(const TROW& row_key, const TCOL& col_key) {
			return table_.at(row_key).at(col_key);
		}

		const TCell& at(const TROW& row_key, const TCOL& col_key) const {
			return table_.at(row_key).at(col_key);
		}

		IteratorType begin() noexcept { return table_.begin(); }
		IteratorType end() noexcept { return table_.end(); }

		ConstIteratorType cbegin() const noexcept { return table_.cbegin(); }
		ConstIteratorType cend() const noexcept { return table_.cend(); }

	private:
		std::map<TROW, std::map<TCOL, TCell>> table_;
	};
}
