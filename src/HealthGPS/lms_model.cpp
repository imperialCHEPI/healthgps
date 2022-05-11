#include "lms_model.h"

#include "fmt/core.h"

namespace hgps {
	LmsModel::LmsModel(LmsDefinition& definition)
		: definition_{ definition } {

		auto& local = definition_.get();
		if (local.min_age() > child_cutoff_age_ || local.max_age() < child_cutoff_age_) {
			throw std::invalid_argument(fmt::format("Child cutoff age outside the LMS valid range: [{}, {}]",
				local.min_age(), local.max_age()));
		}
	}

	unsigned int LmsModel::child_cutoff_age() const noexcept {
		return child_cutoff_age_;
	}

	WeightCategory LmsModel::classify_weight(const Person& entity) const
	{
		auto bmi = entity.get_risk_factor_value(bmi_key_);
		return classify_weight_bmi(entity, bmi);
	}

	double hgps::LmsModel::adjust_risk_factor_value(const Person& entity, std::string risk_factor_key, double value) const
	{
		if (risk_factor_key != bmi_key_) {
			return value;
		}

		if (entity.age <= child_cutoff_age_) {
			auto category = classify_weight_bmi(entity, value);
			switch (category)
			{
			case hgps::WeightCategory::normal:
				return 22.5;
			case hgps::WeightCategory::overweight:
				return 27.5;
			case hgps::WeightCategory::obese:
				return 35.0;
			default:
				throw std::logic_error("Unknown weight category definition.");
			}
		}

		return value;
	}

	WeightCategory LmsModel::classify_weight_bmi(const Person& entity, double bmi) const
	{
		if (entity.age <= child_cutoff_age_)
		{
			auto& params = definition_.get().at(entity.age, entity.gender);

			auto zscore = 0.0;
			if (params.lambda == 0.0) {
				zscore = std::log(bmi / params.mu) / params.sigma;
			}
			else {
				zscore = (std::pow(bmi / params.mu, params.lambda) - 1.0) / (params.lambda * params.sigma);
			}

			if (zscore > 2.0) {
				return WeightCategory::obese;
			}
			else if (zscore > 1.0) {
				return WeightCategory::overweight;
			}
		}
		else {
			if (bmi >= 30.0) {
				return WeightCategory::obese;
			}
			else if (bmi >= 25.0) {
				return WeightCategory::overweight;
			}
		}

		return WeightCategory::normal;
	}
}