#include "model_result.h"
#include <sstream>
#include <format>

namespace hgps {

	std::string ModelResult::to_string() const noexcept {
		std::stringstream ss;

		auto pad = std::max<std::size_t>(24, caluclate_min_padding()) + 2;
		ss << std::format("Average Age: Male = {:.5f}, Female = {:.5f}\n", average_age.male, average_age.female);

		ss << std::format("Indicators.: YLL= {:.5f}, YLD = {:.5f}, DALY = {:.5f}\n", indicators.years_of_life_lost,
			indicators.years_lived_with_disability, indicators.disablity_adjusted_life_years);

		ss << std::format("{:{}}  : {:>14} : {:>14}\n", "#Risk factors average", pad, "Male", "Female");
		for (auto& item : risk_ractor_average) {
			ss << std::format(" {:{}} : {:>14.5f} : {:>14.5f}\n", item.first, pad, item.second.male, item.second.female);
		}

		ss << std::format("{:{}}  : {:>14} : {:>14}\n", "#Prevalence", pad, "Male", "Female");
		for (auto& item : disease_prevalence) {
			ss << std::format(" {:{}} : {:>14.5f} : {:>14.5f}\n", item.first, pad, item.second.male, item.second.female);
		}

		return ss.str();
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
