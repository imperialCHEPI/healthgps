#include "lms_model.h"

#include "fmt/core.h"

namespace hgps {
	LmsModel::LmsModel(LmsDefinition&& definition)
		: definition_{ std::move(definition) } {

		if (definition_.min_age() > child_cutoff_age_ || definition_.max_age() < child_cutoff_age_) {
			throw std::invalid_argument(fmt::format("Child cutoff age outside the LMS valid range: [{}, {}]",
				definition_.min_age(), definition_.max_age()));
		}
	}

	unsigned int LmsModel::child_cutoff_age() const noexcept {
		return child_cutoff_age_;
	}

	WeightCategory LmsModel::classify_weight(const Person& entity) const
	{
		auto bmi = entity.get_risk_factor_value("bmi");
		if (entity.age <= child_cutoff_age_)
		{
			auto& params = definition_.at(entity.age, entity.gender);

			auto zscore = 0.0;
			if (params.lambda == 0.0) {
				zscore = std::log(bmi / params.mu) / params.sigma;
			}
			else {
				zscore = (std::pow(bmi / params.mu, params.lambda) - 1.0) / (params.lambda * params.sigma);
			}

			if (zscore < -2.0) {
				return WeightCategory::underweight;
			}
			else if (zscore > 2.0) {
				return WeightCategory::obese;
			}
			else if (zscore > 1.0) {
				return WeightCategory::overweight;
			}
		}
		else {
			if (bmi < 18.5) {
				return WeightCategory::underweight;
			}
			else if (bmi >= 30.0) {
				return WeightCategory::obese;
			}
			else if (bmi >= 25.0) {
				return WeightCategory::overweight;
			}
		}

		return WeightCategory::normal;
	}
}