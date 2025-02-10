#pragma once
#include "HealthGPS.Core/identifier.h"
#include "person.h"
#include "weight_category.h"

#include <memory>
#include <string>

namespace hgps {

/// @brief Weight classification model polymorphic wrapper class
class WeightModel {
  public:
    /// @brief Initialises a new instance of the WeightModel class.
    /// @tparam T Weight model type
    /// @param value The weight model instance
    template <typename T> WeightModel(T &&value) : pimpl_{new Model<T>(std::forward<T>(value))} {}

    /// @brief Constructs the WeightModel with the copy of the other's contents
    /// @param other The other WeightModel instance to copy
    WeightModel(const WeightModel &other) : pimpl_{other.pimpl_->clone()} {}

    /// @brief Replaces the WeightModel with a copy of the other's contents.
    /// @param other The other WeightModel instance to copy
    /// @return This instance
    WeightModel &operator=(const WeightModel &other) {
        this->pimpl_ = other.pimpl_->clone();
        return *this;
    }

    /// @brief Constructs the WeightModel with the contents of other using move semantics.
    /// @param other The other WeightModel instance to move
    WeightModel(WeightModel &&other) = default;

    /// @brief Replaces the WeightModel contents with the other using move semantics
    /// @param other The other WeightModel instance to move
    /// @return This instance
    WeightModel &operator=(WeightModel &&other) = default;

    /// @brief Gets the children cut-off age (before adult)
    /// @return The cut-off age for children
    unsigned int child_cutoff_age() const noexcept { return pimpl_->child_cutoff_age(); }

    /// @brief Classify a person weight according with the predefined categories
    /// @param person The Person instance to classify
    /// @return The respective weight classification
    WeightCategory classify_weight(const Person &person) const {
        return pimpl_->classify_weight(person);
    }

    /// @brief Adjust a Person risk factor value
    /// @param entity The Person instance to adjust
    /// @param risk_factor_key The risk factor identifier
    /// @param value The amount of adjustment value
    /// @return The adjusted risk factor value
    /// @throws std::out_of_range for unknown weight category definition
    double adjust_risk_factor_value(const Person &entity, const core::Identifier &risk_factor_key, double value) const 
    {
        return pimpl_->adjust_risk_factor_value(entity, risk_factor_key, value);
    }

  private:
    struct Concept {
        virtual ~Concept() {}
        virtual std::unique_ptr<Concept>    clone() const = 0;
        virtual unsigned int                child_cutoff_age() const noexcept = 0;
        virtual WeightCategory              classify_weight(const Person &entity) const = 0;
        virtual double                      adjust_risk_factor_value(const Person &entity, const core::Identifier &risk_factor_key, double value) const = 0;
    };

    template <typename T> struct Model : Concept {

        Model(T &&value) : object_{std::forward<T>(value)} {}

        std::unique_ptr<Concept> clone() const override { return std::make_unique<Model>(*this); }

        unsigned int child_cutoff_age() const noexcept override 
        {
            return object_.child_cutoff_age();
        }

        WeightCategory classify_weight(const Person &entity) const override 
        {
            return object_.classify_weight(entity);
        }

        double adjust_risk_factor_value(const Person &entity,
                                        const core::Identifier &risk_factor_key,
                                        double value) const override 
        {
            return object_.adjust_risk_factor_value(entity, risk_factor_key, value);
        }

        T object_;
    };

    std::unique_ptr<Concept> pimpl_;
};

/// @brief Converts a WeightCategory to a string representation
/// @param value Enumeration value to convert
/// @return The equivalent string
/// @throws std::invalid_argument for unknown weight category value.
std::string weight_category_to_string(WeightCategory value);
} // namespace hgps
