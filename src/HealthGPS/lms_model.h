#pragma once

#include "lms_definition.h"
#include "person.h"

#include "HealthGPS.Core/identifier.h"
#include <functional>
#include <string>

namespace hgps {

/// @brief Implements the LMS (lambda-mu-sigma) model for children growth
class LmsModel {
  public:
    LmsModel() = delete;

    /// @brief Initialises a new instance of the LmsModel class.
    /// @param definition The model definition
    /// @throws std::invalid_argument for definition that don't include the child cut-off age.
    LmsModel(LmsDefinition &definition);

    /// @brief Gets the children cut-off age (before adult)
    /// @return The cut-off age for children
    unsigned int child_cutoff_age() const noexcept;

    /// @brief Classify a person weight according with the predefined categories
    /// @param entity The Person instance to classify
    /// @return The respective weight classification
    WeightCategory classify_weight(const Person &entity) const;

    /// @brief Adjust a Person risk factor value
    /// @param entity The Person instance to adjust
    /// @param factor_key The risk factor identifier
    /// @param value The amount of adjustment value
    /// @return The adjusted risk factor value
    /// @throws std::out_of_range for unknown weight category definition
    double adjust_risk_factor_value(const Person &entity, const core::Identifier &factor_key,
                                    double value) const;

  private:
    std::reference_wrapper<LmsDefinition> definition_;
    unsigned int child_cutoff_age_{18};
    core::Identifier bmi_key_{"bmi"};

    WeightCategory classify_weight_bmi(const Person &entity, double bmi) const;
};
} // namespace hgps
