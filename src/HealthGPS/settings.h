#pragma once

#include <HealthGPS.Core/interval.h>
#include <HealthGPS.Core/poco.h>

namespace hgps {

/// @brief Defines the simulation experiment settings data type
class Settings {
  public:
    /// @brief Initialises a new instance of the Settings class
    /// @param country Target country population
    /// @param size_fraction Virtual population fraction of the real population
    /// @param age_range Allowed age range in the population
    /// @throws std::out_of_range for size fraction value outside of the range (0, 1].
    Settings(core::Country country, const float size_fraction,
             const core::IntegerInterval &age_range);

    /// @brief Gets the experiment target country information
    /// @return Country information
    const core::Country &country() const noexcept;

    /// @brief The virtual population fraction from the real population size in range (0, 1]
    /// @return The size fraction in range (0, 1]
    const float &size_fraction() const noexcept;

    /// @brief The experiment population age range constraint
    /// @return The allowed age range
    const core::IntegerInterval &age_range() const noexcept;

  private:
    core::Country country_;
    float size_fraction_{};
    core::IntegerInterval age_range_;
};
} // namespace hgps