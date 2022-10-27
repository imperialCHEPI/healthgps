#pragma once
#include "person.h"
#include "weight_category.h"
#include "HealthGPS.Core/identifier.h"

#include <memory>
#include <string>

namespace hgps {

    class WeightModel
    {
    public:
        template<typename T>
        WeightModel(T&& value)
            : pimpl_{ new Model<T>(std::forward<T>(value)) } {}

        WeightModel(const WeightModel& s) 
            : pimpl_{ s.pimpl_->clone() } {}

        WeightModel& operator=(const WeightModel& s) {
            this->pimpl_ = s.pimpl_->clone();
            return *this;
        }

        WeightModel(WeightModel&&) = default;
        WeightModel& operator=(WeightModel&&) = default;

        unsigned int child_cutoff_age() const noexcept {
            return pimpl_->child_cutoff_age();
        }

        WeightCategory classify_weight(const Person& person) const {
            return pimpl_->classify_weight(person);
        }

        double adjust_risk_factor_value(const Person& entity,
            const core::Identifier& risk_factor_key, double value) const {
            return pimpl_->adjust_risk_factor_value(entity, risk_factor_key, value);
        }

    private:
        struct Concept
        {
            virtual ~Concept() {}
            virtual std::unique_ptr<Concept> clone() const = 0;
            virtual unsigned int child_cutoff_age() const noexcept = 0;
            virtual WeightCategory classify_weight(const Person& entity) const = 0;
            virtual double adjust_risk_factor_value(const Person& entity,
                const core::Identifier& risk_factor_key, double value) const = 0;
        };

        template<typename T>
        struct Model : Concept
        {
            Model(T&& value) : object_{ std::forward<T>(value) } {}

            std::unique_ptr<Concept> clone() const override {
                return std::make_unique<Model>(*this);
            }

            unsigned int child_cutoff_age() const noexcept override {
                return object_.child_cutoff_age();
            }

            WeightCategory classify_weight(const Person& entity) const override {
                return object_.classify_weight(entity);
            }

            double adjust_risk_factor_value(const Person& entity,
                const core::Identifier& risk_factor_key, double value) const override {
                return object_.adjust_risk_factor_value(entity, risk_factor_key, value);
            }

            T object_;
        };

        std::unique_ptr<Concept> pimpl_;
    };

    std::string weight_category_to_string(WeightCategory value);
}
