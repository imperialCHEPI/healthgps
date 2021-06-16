#include "datatable.h"
#include <stdexcept>

namespace hgps {
	namespace data {
		const std::size_t DataTable::num_columns() const noexcept { return columns_.size(); }

		const std::size_t DataTable::num_rows() const noexcept { return rows_count_; }

		const std::vector<std::string> DataTable::names() const { return names_; }

		void DataTable::add(std::unique_ptr<DataTableColumn> column) {

			std::scoped_lock lk(sync_mtx_);

			if ((rows_count_ > 0 && column->length() != rows_count_) ||
				(num_columns() > 0 && rows_count_ == 0 && column->length() != rows_count_)) {

				throw std::invalid_argument(
					"Column lengths mismatch, new columns must have the same lengths of existing ones.");
			}

			auto column_key = toLower(column->name());
			if (index_.contains(column_key)) {
				throw std::invalid_argument("Duplicated column name is not allowed.");
			}

			rows_count_ = column->length();
			names_.push_back(column->name());
			index_[column_key] = columns_.size();
			columns_.push_back(std::move(column));
		}

		const std::optional<std::reference_wrapper<DataTableColumn>> DataTable::column(std::size_t index)
		{
			if (index < num_columns()) {
				// Reference to pointed object
				return *columns_.at(index);
			}

			return std::nullopt;
		}

		const std::optional<std::reference_wrapper<DataTableColumn>> DataTable::column(std::string name)
		{
			auto lower_name = toLower(name);
			auto found = index_.find(lower_name);
			if (found != index_.end()) {
				// Reference to pointed object
				return *columns_.at(found->second);
			}

			return std::nullopt;
		}

		std::string DataTable::to_string() const
		{
			std::stringstream ss;
			std::size_t longestColumnName = 0;
			for (auto& col : columns_) {
				longestColumnName = std::max(longestColumnName, col->name().length());
			}

			auto pad = longestColumnName + 2;
			auto width = pad + 28;

			ss << std::format("\n Table size: {} x {}\n", num_columns(), num_rows());
			ss << std::format("|{:-<{}}|\n", '-', width);
			ss << std::format("| {:{}} : {:10} : {:>10} |\n", "Column Name", pad, "Data Type", "# Nulls");
			ss << std::format("|{:-<{}}|\n", '-', width);
			for (auto& col : columns_)
			{
				ss << std::format("| {:{}} : {:10} : {:10} |\n",
					col->name(), pad,
					col->type(),
					col->null_count());
			}

			ss << std::format("|{:_<{}}|\n\n", '_', width);

			return ss.str();
		}
	}
}