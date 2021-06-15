#include "datatable.h"
#include <stdexcept>

namespace hgps {
	namespace data {
		const std::size_t DataTable::count() const noexcept {
			return columns_.size();
		}

		const std::vector<std::string> DataTable::names() const {
			return names_;
		}

		void DataTable::add(std::unique_ptr<DataTableColumn> column) {

			std::scoped_lock lk(sync_mtx_);

			if ((row_count_ > 0 && column->length() != row_count_) ||
				(count() > 0 && row_count_ == 0 && column->length() != row_count_))	{

				throw std::invalid_argument(
					"Column lengths mismatch, new columns must have the same lengths of existing ones.");
			}

			auto column_key = toLower(column->name());
			if (index_.contains(column_key)) {
				throw std::invalid_argument("Duplicated column name is not allowed.");
			}

			row_count_ = column->length();
			names_.push_back(column->name());
			index_[column_key] = columns_.size();
			columns_.push_back(std::move(column));
		}

		const std::optional<std::reference_wrapper<DataTableColumn>> DataTable::column(std::size_t index)
		{
			if (index < count()) {
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
			PrettyPrint(*this, 0, &ss));
			return ss.str();
		}
	}
}