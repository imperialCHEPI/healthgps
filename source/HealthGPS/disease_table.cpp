#include "disease_table.h"
#include "HealthGPS.Core/string_util.h"

#include <stdexcept>
#include <fmt/format.h>

namespace hgps {
	/* --------------------   Disease Measure Implementation ----------------- */

	DiseaseMeasure::DiseaseMeasure(std::map<int, double> measures)
		: measures_{ measures } {}

	std::size_t DiseaseMeasure::size() const noexcept {
		return measures_.size();
	}

	const double& DiseaseMeasure::at(const int measure_id) const {
		return measures_.at(measure_id);
	}

	const double& DiseaseMeasure::operator[](const int measure_id) const {
		return measures_.at(measure_id);
	}

	/* --------------------   Disease Table Implementation ----------------- */


	DiseaseTable::DiseaseTable(core::DiseaseInfo info, std::map<std::string, int>&& measures,
		std::map<int, std::map<core::Gender, DiseaseMeasure>>&& data)
		: info_{ info }, measures_{ measures }, data_{ data } {

		if (info.code.empty()) {
			throw std::invalid_argument("Invalid disease information with empty identifier");
		}
		else if (data.empty()) {
			return; // empty table
		}

		// Consistence checks, otherwise number of columns is wrong.
		auto col_size = data.begin()->second.size();
		for (auto& age : data) {
			if (age.second.size() != col_size) {
				throw std::invalid_argument(
					fmt::format("Number of columns mismatch at age: {} ({} vs {}).",
						age.first, age.second.size(), col_size));
			}
		}
	}

	const core::DiseaseInfo& DiseaseTable::info() const noexcept { return info_; }

	std::size_t DiseaseTable::size() const noexcept { return rows() * cols(); }

	std::size_t DiseaseTable::rows() const noexcept { return data_.size(); }

	std::size_t DiseaseTable::cols() const noexcept {
		if (data_.empty()) {
			return 0;
		}

		return data_.begin()->second.size();
	}

	bool DiseaseTable::contains(const int age) const noexcept {
		return data_.contains(age);
	}

	std::map<std::string, int> DiseaseTable::measures() const noexcept {
		return measures_;
	}

	int DiseaseTable::at(const std::string measure) const	{
		return measures_.at(core::to_lower(measure)); 
	}

	int DiseaseTable::operator[](const std::string measure) const {
		return measures_.at(core::to_lower(measure));
	}

	DiseaseMeasure& DiseaseTable::operator()(const int age, const core::Gender gender) {
		return data_.at(age).at(gender);
	}

	const DiseaseMeasure& DiseaseTable::operator()(const int age, const core::Gender gender) const {
		return data_.at(age).at(gender);
	}

}
