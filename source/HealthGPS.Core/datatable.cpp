#include "datatable.h"
#include "string_util.h"
#include <fmt/format.h>

#include <stdexcept>
#include <sstream>

namespace hgps {
	namespace core {
		std::size_t DataTable::num_columns() const noexcept { return columns_.size(); }

		std::size_t DataTable::num_rows() const noexcept { return rows_count_; }

		std::vector<std::string> DataTable::names() const { return names_; }

		void DataTable::add(std::unique_ptr<DataTableColumn> column) {

			std::scoped_lock lk(sync_mtx_);

			if ((rows_count_ > 0 && column->length() != rows_count_) ||
				(num_columns() > 0 && rows_count_ == 0 && column->length() != rows_count_)) {

				throw std::invalid_argument(
					"Column lengths mismatch, new columns must have the same lengths of existing ones.");
			}

			auto column_key = to_lower(column->name());
			if (index_.contains(column_key)) {
				throw std::invalid_argument("Duplicated column name is not allowed.");
			}

			rows_count_ = column->length();
			names_.push_back(column->name());
			index_[column_key] = columns_.size();
			columns_.push_back(std::move(column));
		}

		const DataTableColumn& DataTable::column(std::size_t index) const
		{
			if (index < num_columns()) {
				return *columns_.at(index);
			}

			throw std::invalid_argument("Column index outside the range.");
		}

		const DataTableColumn& DataTable::column(std::string name) const
		{
			auto lower_name = to_lower(name);
			auto found = index_.find(lower_name);
			if (found != index_.end()) {
				return *columns_.at(found->second);
			}

			throw std::invalid_argument(fmt::format("Column name: {} not found.", name));
		}

		std::string DataTable::to_string() const
		{
			std::stringstream ss;
			std::size_t longestColumnName = 0;
			for (auto& col : columns_) {
				longestColumnName = std::max(longestColumnName, col->name().length());
			}

			auto pad = longestColumnName + 4;
			auto width = pad + 28;

			ss << fmt::format("\n Table size: {} x {}\n", num_columns(), num_rows());
			ss << fmt::format("|{:-<{}}|\n", '-', width);
			ss << fmt::format("| {:{}} : {:10} : {:>10} |\n", "Column Name", pad, "Data Type", "# Nulls");
			ss << fmt::format("|{:-<{}}|\n", '-', width);
			for (auto& col : columns_)
			{
				ss << fmt::format("| {:{}} : {:10} : {:10} |\n",
					col->name(), pad,
					col->type(),
					col->null_count());
			}

			ss << fmt::format("|{:_<{}}|\n\n", '_', width);

			return ss.str();
		}
	}
}