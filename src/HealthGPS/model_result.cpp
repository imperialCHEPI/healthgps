#include "model_result.h"
#include <sstream>
#include <fmt/format.h>

namespace hgps {

	ModelResult::ModelResult(const unsigned int sample_size) 
		: time_series{sample_size} {}

	std::string ModelResult::to_string() const noexcept {
		std::stringstream ss;

		auto pad = std::max<std::size_t>(24, caluclate_min_padding()) + 2;
		ss << fmt::format("Population.: {}, alive = {}, migrating = {}, dead = {}\n",
			population_size, number_alive, number_emigrated, number_dead);

		ss << fmt::format("Average Age: Male = {:.5f}, Female = {:.5f}\n", average_age.male, average_age.female);

		ss << fmt::format("Indicators.: YLL= {:.5f}, YLD = {:.5f}, DALY = {:.5f}\n", indicators.years_of_life_lost,
			indicators.years_lived_with_disability, indicators.disablity_adjusted_life_years);

		ss << fmt::format("{:{}}  : {:>14} : {:>14}\n", "#Risk factors average", pad, "Male", "Female");
		for (auto& item : risk_ractor_average) {
			ss << fmt::format(" {:{}} : {:>14.5f} : {:>14.5f}\n", item.first, pad, item.second.male, item.second.female);
		}

		ss << fmt::format("{:{}}  : {:>14} : {:>14}\n", "#Prevalence", pad, "Male", "Female");
		for (auto& item : disease_prevalence) {
			ss << fmt::format(" {:{}} : {:>14.5f} : {:>14.5f}\n", item.first, pad, item.second.male, item.second.female);
		}

		return ss.str();
	}

	int ModelResult::number_of_recyclable() const noexcept {
		return population_size - (number_alive + number_dead + number_emigrated);
	}

	std::size_t ModelResult::caluclate_min_padding() const noexcept {
		std::size_t longestName = 0;
		for (const auto& entry : risk_ractor_average) {
			longestName = std::max(longestName, entry.first.length());
		}

		for (const auto& item : disease_prevalence) {
			longestName = std::max(longestName, item.first.length());
		}

		return longestName;
	}
}
