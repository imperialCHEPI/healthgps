#include "weight_model.h"

namespace hgps {
	std::string weight_category_to_string(WeightCategory value)
	{
		switch (value)
		{
		case WeightCategory::underweight:
			return "underweight";
		case WeightCategory::normal:
			return "normal";
		case WeightCategory::overweight:
			return "overweight";
		case WeightCategory::obese:
			return "obese";
		default:
			throw std::invalid_argument("Unknown weight category item");
		}
	}
}