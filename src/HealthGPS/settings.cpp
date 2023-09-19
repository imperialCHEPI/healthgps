#include <stdexcept>
#include <utility>

#include "settings.h"

namespace hgps {

Settings::Settings(core::Country country, const float size_fraction,
                   const core::IntegerInterval &age_range)
    : country_{std::move(country)}, size_fraction_{size_fraction}, age_range_{age_range} {

    // TODO: Create a fraction type wrapper
    if (size_fraction <= 0.0 || size_fraction > 1.0) {
        throw std::out_of_range(
            "Invalid population size fraction value, must be between in range (0, 1].");
    }
}

const core::Country &Settings::country() const noexcept { return country_; }

float Settings::size_fraction() const noexcept { return size_fraction_; }

const core::IntegerInterval &Settings::age_range() const noexcept { return age_range_; }
} // namespace hgps
