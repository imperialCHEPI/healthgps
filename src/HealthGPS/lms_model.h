#pragma once

#include "person.h"
#include "lms_definition.h"

#include <string>
#include "HealthGPS.Core/identifier.h"
#include <functional>

namespace hgps {

	class LmsModel	{
    public:
        LmsModel() = delete;
        LmsModel(LmsDefinition& definition);
        unsigned int child_cutoff_age() const noexcept;
        WeightCategory classify_weight(const Person& entity) const;
        double adjust_risk_factor_value(const Person& entity, const core::Identifier& factor_key, double value) const;

    private:
        std::reference_wrapper<LmsDefinition> definition_;
        unsigned int child_cutoff_age_{18};
        core::Identifier bmi_key_{ "bmi" };

        WeightCategory classify_weight_bmi(const Person& entity, double bmi) const;
	};
}
