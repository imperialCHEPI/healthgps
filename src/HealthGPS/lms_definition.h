#pragma once
#include "HealthGPS.Core/forward_type.h"
#include "weight_category.h"

namespace hgps {

    /// @brief Defines the LMS (lambda-mu-sigma) parameters record data type
    struct LmsRecord {
        /// @brief Lambda parameter value
        double lambda{};
        
        /// @brief Mu parameter value
        double mu{};

        /// @brief Sigma parameter value
        double sigma{};
    };

    /// @brief Defines the LMS (lambda-mu-sigma) model dataset
    using LmsDataset = std::map<unsigned int, std::map<core::Gender, LmsRecord>>;

    /// @brief LMS (lambda-mu-sigma) model definition data type
    class LmsDefinition
    {
    public:
        /// @brief Initialises a new instance of the LmsDefinition class.
        LmsDefinition() = default;

        /// @brief Initialises a new instance of the LmsDefinition class.
        /// @param dataset The dataset instance to initialise
        /// @throws std::invalid_argument for empty LMS dataset definition.
        LmsDefinition(LmsDataset&& dataset) 
            : table_{ std::move(dataset) }{

            if (table_.empty()) {
                throw std::invalid_argument("The LMS definition must not be empty.");
            }
        }

        /// @brief Determine whether the MLS definition is empty
        /// @return true if the definition is empty; otherwise, false
        bool empty() const noexcept { return table_.empty(); }

        /// @brief Gets the size of the MLS definition dataset
        /// @return The size of the definition dataset
        std::size_t size() const noexcept { return table_.size(); }

        /// @brief Gets the definition minimum age
        /// @return Minimum age value
        unsigned int min_age() const noexcept { return table_.cbegin()->first; }

        /// @brief Gets the definition maximum age
        /// @return Maximum age value
        unsigned int max_age() const noexcept { return table_.rbegin()->first; }

        /// @brief Determine whether the definition contains a given age by gender
        /// @param age The age reference
        /// @param gender The gender reference
        /// @return true if the definition contains the age; otherwise, false
        bool contains(unsigned int age, core::Gender gender) const noexcept {
            if (table_.contains(age)) {
                return table_.at(age).contains(gender);
            }

            return false;
        }

        /// @brief Gets the model parameters for a given age and gender
        /// @param age The age reference
        /// @param gender The gender reference
        /// @return The model parameters
        /// @throws std::out_of_range if the definition does not contains the specified age or gender
        const LmsRecord& at(unsigned int age, core::Gender gender) const {
            return table_.at(age).at(gender);
        }

    private:
        LmsDataset table_{};
    };
}