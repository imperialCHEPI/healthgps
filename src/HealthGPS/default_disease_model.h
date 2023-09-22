#pragma once
#include "disease_definition.h"
#include "gender_table.h"
#include "interfaces.h"
#include "weight_model.h"

namespace hgps {
/// @brief Defines the standard diseases (non-cancer) model data type.
class DefaultDiseaseModel final : public DiseaseModel {
  public:
    DefaultDiseaseModel() = delete;

    /// @brief Initialise a new instance of the DefaultDiseaseModel class.
    /// @param definition The disease definition instance
    /// @param classifier The body weight classification model instance
    /// @param age_range The valid age range for model dataset
    DefaultDiseaseModel(DiseaseDefinition &definition, WeightModel &&classifier,
                        const core::IntegerInterval &age_range);

    core::DiseaseGroup group() const noexcept override;

    const core::Identifier &disease_type() const noexcept override;

    void initialise_disease_status(RuntimeContext &context) override;

    void initialise_average_relative_risk(RuntimeContext &context) override;

    void update_disease_status(RuntimeContext &context) override;

    double get_excess_mortality(const Person &entity) const noexcept override;

  private:
    std::reference_wrapper<DiseaseDefinition> definition_;
    WeightModel weight_classifier_;
    DoubleAgeGenderTable average_relative_risk_;

    DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext &context);
    double calculate_combined_relative_risk(const Person &entity, int start_time,
                                            int time_now) const;
    double calculate_relative_risk_for_diseases(const Person &entity, int start_time,
                                                int time_now) const;
    double calculate_relative_risk_for_risk_factors(const Person &entity) const;
    double calculate_incidence_probability(const Person &entity, int start_time,
                                           int time_now) const;

    void update_remission_cases(RuntimeContext &context);
    void update_incidence_cases(RuntimeContext &context);
};
} // namespace hgps
