#pragma once

#include "analysis_definition.h"
#include "interfaces.h"
#include "modelinput.h"
#include "repository.h"
#include "result_message.h"
#include "runtime_context.h"
#include "weight_model.h"

namespace hgps {

/// @brief Implements the burden of diseases (BoD) analysis module
class AnalysisModule final : public UpdatableModule {
  public:
    AnalysisModule() = delete;

    /// @brief Initialises a new instance of the AnalysisModule class.
    /// @param definition The analysis module definition
    /// @param classifier Body weight classifier model instance
    /// @param age_range The experiment valid age range
    /// @param comorbidities Maximum number of comorbidities to include
    AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                   const core::IntegerInterval age_range, unsigned int comorbidities);

    /// @brief Initialises a new instance of the AnalysisModule class.
    /// @param definition The analysis module definition
    /// @param classifier Body weight classifier model instance
    /// @param age_range The experiment valid age range
    /// @param comorbidities Maximum number of comorbidities to include
    /// @param calculated_factors The calculated factors to include
    AnalysisModule(AnalysisDefinition &&definition, WeightModel &&classifier,
                   const core::IntegerInterval age_range, unsigned int comorbidities,
                   std::vector<double> calculated_factors);

    SimulationModuleType type() const noexcept override;

    const std::string &name() const noexcept override;

    void initialise_population(RuntimeContext &context) override;

    void update_population(RuntimeContext &context) override;

  private:
    AnalysisDefinition definition_;
    WeightModel weight_classifier_;
    DoubleAgeGenderTable residual_disability_weight_;
    std::vector<std::string> channels_;
    unsigned int comorbidities_;
    std::string name_{"Analysis"};
    std::vector<core::Identifier> factors_to_calculate_ = {"Gender"_id, "Age"_id};
    std::vector<double> calculated_factors_;
    std::vector<int> factor_bins_;
    std::vector<double> factor_bin_widths_;
    std::vector<double> factor_min_values_;

    void initialise_vector(RuntimeContext &context);

    double calculate_residual_disability_weight(int age, core::Gender gender,
                                                const DoubleAgeGenderTable &expected_sum,
                                                const IntegerAgeGenderTable &expected_count);

    void publish_result_message(RuntimeContext &context) const;
    void calculate_historical_statistics(RuntimeContext &context, ModelResult &result) const;
    double calculate_disability_weight(const Person &entity) const;
    DALYsIndicator calculate_dalys(Population &population, unsigned int max_age,
                                   unsigned int death_year) const;

    void calculate_population_statistics(RuntimeContext &context) const;
    void calculate_population_statistics(RuntimeContext &context, DataSeries &series) const;

    void classify_weight(hgps::DataSeries &series, const hgps::Person &entity) const;
    void initialise_output_channels(RuntimeContext &context);

    /// @brief Calculates the standard deviation of factors given data series containing means
    /// @param context The runtime context
    /// @param series The data series containing factor means
    void calculate_standard_deviation(RuntimeContext &context, DataSeries &series) const;
};

/// @brief Builds a new instance of the AnalysisModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new AnalysisModule instance
/// @throws std::logic_error for invalid disease analysis definition
std::unique_ptr<AnalysisModule> build_analysis_module(Repository &repository,
                                                      const ModelInput &config);
} // namespace hgps
