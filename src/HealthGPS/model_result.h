#pragma once
#include "data_series.h"
#include "gender_value.h"

#include <map>
#include <string>
#include <vector>

namespace hgps {

/// @brief Defines the measure by gender result data type
struct ResultByGender {

    /// @brief Male value
    double male{};

    /// @brief Female value
    double female{};
};

/// @brief Defines the DALYs indicator result data type
struct DALYsIndicator {
    /// @brief Years of life lost (YLL) value
    double years_of_life_lost{};

    /// @brief Years lived with disability (YLD) value
    double years_lived_with_disability{};

    /// @brief Disability adjusted life years (DALY) value
    double disability_adjusted_life_years{};
};

/// @brief Defines the model results data type
struct ModelResult {
    ModelResult() = delete;

    /// @brief Initialises a new instance of the ModelResult structure
    /// @param sample_size The results sample size value
    ModelResult(const unsigned int sample_size);

    /// @brief Total population size
    int population_size{};

    /// @brief The number of people alive in the population
    IntegerGenderValue number_alive{};

    /// @brief The number of dead in the population
    int number_dead{};

    /// @brief The number of emigrants in the population
    int number_emigrated{};

    /// @brief Average age of the population
    ResultByGender average_age{};

    /// @brief The DALYs indicator
    DALYsIndicator indicators{};

    /// @brief Average risk factors value in population
    std::map<std::string, ResultByGender> risk_ractor_average{};

    /// @brief Average diseases prevalence value in population
    std::map<std::string, ResultByGender> disease_prevalence{};

    /// @brief Average comorbidity in population
    std::map<unsigned int, ResultByGender> comorbidity{};

    /// @brief Simulation runtime metrics
    std::map<std::string, double> metrics{};

    /// @brief The detailed time series data results
    DataSeries series;

    /// @brief Gets a string representation of this instance
    /// @return The string representation
    std::string to_string() const noexcept;

    /// @brief Gets the number of recycled population slots due to deaths and emigration
    /// @return Number of recycled population slots
    int number_of_recyclable() const noexcept;

  private:
    std::size_t caluclate_min_padding() const noexcept;
};
} // namespace hgps
