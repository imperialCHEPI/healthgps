#include "disease_table.h"
#include <stdexcept>
#include <format>

#include "HealthGPS.Core/string_util.h"

namespace hgps {
	/* --------------------   Disease Measure Implementation ----------------- */

	DiseaseMeasure::DiseaseMeasure(std::map<int, double> measures)
		: measures_{ measures } {}

	std::size_t DiseaseMeasure::size() const noexcept {
		return measures_.size();
	}

	double DiseaseMeasure::at(int measure_id) const {
		return measures_.at(measure_id);
	}

	const double DiseaseMeasure::operator[](int measure_id) const {
		return measures_.at(measure_id);
	}

	/* --------------------   Disease Table Implementation ----------------- */


	DiseaseTable::DiseaseTable(std::string name, std::map<std::string, int>&& measures,
		std::map<int, std::map<core::Gender, DiseaseMeasure>>&& data)
		: name_{ name }, measures_{ measures }, data_{ data } {

		if (data.empty()) {
			return; // empty table
		}

		// Consistence checks, otherwise number of columns is wrong.
		auto col_size = data.begin()->second.size();
		for (auto& age : data) {
			if (age.second.size() != col_size) {
				throw std::invalid_argument(
					std::format("Number of columns mismatch at age: {} ({} vs {}).",
						age.first, age.second.size(), col_size));
			}
		}
	}

	std::string DiseaseTable::name() const noexcept { return name_; }

	std::size_t DiseaseTable::size() const noexcept { return rows() * cols(); }

	std::size_t DiseaseTable::rows() const noexcept { return data_.size(); }

	std::size_t DiseaseTable::cols() const noexcept {
		if (data_.empty()) {
			return 0;
		}

		return data_.begin()->second.size();
	}

	std::map<std::string, int> DiseaseTable::measures() const noexcept {
		return measures_;
	}

	int DiseaseTable::at(std::string measure) const	{
		return measures_.at(core::to_lower(measure)); 
	}

	const int DiseaseTable::operator[](std::string measure) const {
		return measures_.at(core::to_lower(measure));
	}

	DiseaseMeasure& DiseaseTable::operator()(int age, core::Gender gender) {
		return data_.at(age).at(gender);
	}

	const DiseaseMeasure& DiseaseTable::operator()(int age, core::Gender gender) const {
		return data_.at(age).at(gender);
	}

}
