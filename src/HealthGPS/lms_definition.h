#pragma once
#include "HealthGPS.Core/forward_type.h"
#include "weight_category.h"

namespace hgps {

    struct LmsRecord {
        double lambda{};
        double mu{};
        double sigma{};
    };

    using LmsDataset = std::map<unsigned int, std::map<core::Gender, LmsRecord>>;

    class LmsDefinition
    {
    public:
        LmsDefinition() = delete;
        LmsDefinition(LmsDataset&& dataset) 
            : table_{ std::move(dataset) }{

            if (table_.empty()) {
                throw std::invalid_argument("The LMS definition must not be empty.");
            }
        }

        std::size_t size() const noexcept { return table_.size(); }
        unsigned int min_age() const noexcept { return table_.cbegin()->first; }
        unsigned int max_age() const noexcept { return table_.rbegin()->first; }

        bool contains(unsigned int age, core::Gender gender) const noexcept {
            if (table_.contains(age)) {
                return table_.at(age).contains(gender);
            }

            return false;
        }

        const LmsRecord& at(unsigned int age, core::Gender gender) const {
            return table_.at(age).at(gender);
        }

    private:
        LmsDataset table_;
    };
}