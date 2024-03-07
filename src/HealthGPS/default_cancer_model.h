#pragma once
#include "disease_definition.h"
#include "gender_table.h"
#include "interfaces.h"
#include "weight_model.h"

namespace hgps {

/// @brief Defines the cancer disease model data type.
class DefaultCancerModel final : public DiseaseModel {
  public:
    DefaultCancerModel() = delete;

    /// @brief Initialise a new instance of the DefaultCancerModel class.
    /// @param definition The disease definition instance
    /// @param classifier The body weight classification model instance
    /// @param age_range The valid age range for model dataset
    DefaultCancerModel(DiseaseDefinition &definition, WeightModel &&classifier,
                       const core::IntegerInterval &age_range);

    core::DiseaseGroup group() const noexcept override;

    const core::Identifier &disease_type() const noexcept override;

    void initialise_disease_status(RuntimeContext &context) override;

    void initialise_average_relative_risk(RuntimeContext &context) override;

    void update_disease_status(RuntimeContext &context) override;

    double get_excess_mortality(const Person &person) const noexcept override;

  private:
    std::reference_wrapper<DiseaseDefinition> definition_;
    WeightModel weight_classifier_;
    DoubleAgeGenderTable average_relative_risk_;

    DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext &context);

    double calculate_relative_risk_for_diseases(const Person &person) const;

    double calculate_relative_risk_for_risk_factors(const Person &person) const;

    void update_remission_cases(RuntimeContext &context);

    void update_incidence_cases(RuntimeContext &context);

    int calculate_time_since_onset(RuntimeContext &context, const core::Gender &gender) const;
};

} // namespace hgps
