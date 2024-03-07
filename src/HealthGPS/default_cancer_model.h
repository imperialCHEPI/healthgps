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

    /// @brief Returns the disease group type (cancer).
    /// @return The disease group type (cancer).
    core::DiseaseGroup group() const noexcept override;

    /// @brief Returns the cancer type identifier.
    /// @return The cancer type identifier
    const core::Identifier &disease_type() const noexcept override;

    /// @brief Initialises the cancer status of the population (from prevalence).
    /// Cancer start_time = 0 disambiguates from incidence cases on start year.
    /// Must be followed by:
    ///   * initialise_average_relative_risk (used to preserve distribution)
    ///   * update_disease_status (to simulate incidence cases on start year)
    /// @param context The runtime context instance
    void initialise_disease_status(RuntimeContext &context) override;

    /// @brief Initialise average relative risk for cancer model on start year.
    /// The average relative risks from risk factors and diseases on start year
    /// is used to normalise the relative risks for an indivudual at a given time
    /// To preserve the distribution of this cancer as the simulation prorgesses.
    /// @param context The runtime context instance
    void initialise_average_relative_risk(RuntimeContext &context) override;

    /// @brief Simulates cancer remission and incidence cases of the population.
    /// For each year, this method calls the following methods in ordrer:
    ////  * update_remission_cases
    ////  * update_incidence_cases
    /// @param context The runtime context instance
    void update_disease_status(RuntimeContext &context) override;

    /// @brief Returns the excess mortality for a given person's age and sex.
    /// @param person The person instance to calculate the excess mortality
    /// @return The excess mortality for a given person's age and sex
    double get_excess_mortality(const Person &person) const noexcept override;

  private:
    std::reference_wrapper<DiseaseDefinition> definition_;
    WeightModel weight_classifier_;
    DoubleAgeGenderTable average_relative_risk_;

    /// @brief Initialise average relative risk for cancer model on start year.
    /// This method is similar to initialise_average_relative_risk, and used internally
    /// by initialise_disease_status.
    ///
    /// Since disease status is not yet initialised, this method only considers
    /// relative risk from risk factors. We can eventualy bootstrap the final average
    /// relative risk once initialise_disease_status has finished initialising by calling
    /// the initialise_average_relative_risk method.
    /// @param context The runtime context instance
    DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext &context);

    /// @brief Caculates the relative risk for this cancer given other diseases.
    /// @param person The person to calculate the relative risk
    /// @return The relative risk for this cancer given other diseases
    double calculate_relative_risk_for_diseases(const Person &person) const;

    /// @brief Caculates the relative risk for this cancer given risk factors.
    /// @param person The person to calculate the relative risk
    /// @return The relative risk for this cancer given risk factors
    double calculate_relative_risk_for_risk_factors(const Person &person) const;

    /// @brief Simulates the remission cases of the population for this cancer.
    /// Each person may be freed from this cancer status based on time since onset.
    /// @param context The runtime context instance
    void update_remission_cases(RuntimeContext &context);

    /// @brief Simulates the incidence cases of the population for this cancer.
    /// Each person may be affected by this cancer status based on incidence probability
    /// and relative risks from their risk risk factors and other diseases.
    /// @param context The runtime context instance
    void update_incidence_cases(RuntimeContext &context);

    /// @brief Generates a random time since onset during initialisation.
    /// @param context The runtime context instance
    /// @param gender The person's sex
    /// @return the time since cancer onset
    int calculate_time_since_onset(RuntimeContext &context, const core::Gender &gender) const;
};

} // namespace hgps
