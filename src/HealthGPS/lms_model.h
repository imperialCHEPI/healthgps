#pragma once

#include "person.h"
#include "lms_definition.h"

namespace hgps {

	class LmsModel	{
    public:
        LmsModel() = delete;
        LmsModel(LmsDefinition&& definition);
        unsigned int child_cutoff_age() const noexcept;
        WeightCategory classify_weight(const Person& entity) const;

    private:
        LmsDefinition definition_;
        unsigned int child_cutoff_age_{18};
	};
}
