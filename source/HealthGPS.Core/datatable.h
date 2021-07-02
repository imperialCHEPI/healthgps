#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <optional>

#include "column.h"


namespace hgps {
	namespace core {

		class DataTable
		{
		public:
			using IteratorType = std::vector<std::unique_ptr<DataTableColumn>>::const_iterator;

			const std::size_t num_columns() const noexcept;

			const std::size_t num_rows() const noexcept;

			const std::vector<std::string> names() const;

			void add(std::unique_ptr<DataTableColumn> column);

			const std::unique_ptr<DataTableColumn>& column(std::size_t index);

			const std::unique_ptr<DataTableColumn>& column(std::string name);

			IteratorType begin() const noexcept { return columns_.begin(); }

			IteratorType end() const noexcept { return columns_.end(); }

			std::string to_string() const;

		private:
			std::vector<std::string> names_{};
			std::unordered_map < std::string, std::size_t> index_{};
			std::vector<std::unique_ptr<DataTableColumn>> columns_{};
			size_t rows_count_ = 0;
			std::mutex sync_mtx_{};
		};
	}
}

