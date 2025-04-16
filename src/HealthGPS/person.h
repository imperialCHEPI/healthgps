#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <map>
#include <unordered_map>

namespace hgps {

/// @brief Disease status enumeration
enum struct DiseaseStatus : uint8_t {
    /// @brief Declared free from condition
    free,

    /// @brief Current with the condition
    active
};

/// @brief Defines the disease history data type
struct Disease {
    /// @brief The disease current status
    DiseaseStatus status{};

    /// @brief Disease start time
    int start_time{};

    /// @brief Disease time since onset (cancer disease only)
    int time_since_onset{-1};

    /// @brief Clone a disease history entry
    /// @return A new instance of the Disease class
    Disease clone() const noexcept {
        return Disease{
            .status = status, .start_time = start_time, .time_since_onset = time_since_onset};
    }
};

/// @brief Defines a virtual population person data type.
struct Person {
    /// @brief Initialise a new instance of the Person structure
    Person();

    /// @brief Initialise a new instance of the Person structure
    /// @param gender The new person gender
    Person(const core::Gender gender) noexcept;

    /// @brief Gets this instance unique identifier
    /// @note The identifier is unique within a virtual population only, not global unique.
    /// @return Unique identifier
    std::size_t id() const noexcept;

    /// @brief The assigned gender
    core::Gender gender{core::Gender::unknown};

    /// @brief Current age in years
    unsigned int age{};

    /// @brief Sector (region) assigned value
    core::Sector sector{core::Sector::unknown};

    /// @brief Income category
    core::Income income{core::Income::unknown};

    /// @brief Social-economic status (SES) assigned value
    double ses{};

    /// @brief Region assigned value
    core::Region region{core::Region::unknown};

    /// @brief Current risk factors values
    std::unordered_map<core::Identifier, double> risk_factors{};

    /// @brief Diseases history and current status
    std::map<core::Identifier, Disease> diseases;

    /// @brief Demographic attributes
    core::Ethnicity ethnicity{core::Ethnicity::unknown};
    double income_continuous{0.0};
    double physical_activity{0.0};

    /// @brief Determine if a Person is current alive
    /// @return true for alive; otherwise, false
    bool is_alive() const noexcept;

    /// @brief Determine if a Person has emigrated from the population
    /// @return true for current emigrated; otherwise, false.
    bool has_emigrated() const noexcept;

    /// @brief Gets the time of death, for dead, non-alive individuals only
    /// @return Time of death value
    unsigned int time_of_death() const noexcept;

    /// @brief Gets the time of migration, for emigrated individuals only
    /// @return Time of emigration value
    unsigned int time_of_migration() const noexcept;

    /// @brief Gets a value indicating whether a Person is current active in the population
    /// @note A person is active only if still alive and has not emigrated.
    /// @return true for active; otherwise, false.
    bool is_active() const noexcept;

    /// @brief Gets a risk factor current value
    /// @param key The risk factor identifier
    /// @return Current risk factor value, if found
    /// @throws std::out_of_range for unknown risk factor
    double get_risk_factor_value(const core::Identifier &key) const;

    /// @brief Gets the gender enumeration as a number for analysis
    /// @return The gender associated value
    /// @throws HgpsException if gender is unknown
    float gender_to_value() const;

    /// @brief Gets the gender enumeration as a number for analysis
    /// @param gender The gender to convert to a value
    /// @return The gender associated value
    /// @throws HgpsException if gender is unknown
    static float gender_to_value(core::Gender gender);

    /// @brief Gets the gender enumeration name string
    /// @return The gender name
    /// @throws HgpsException if gender is unknown
    std::string gender_to_string() const;

    /// @brief Check if person is an adult (18 or over)
    /// @return true if person is 18 or over; else false
    bool over_18() const noexcept;

    /// @brief Gets the sector enumeration as a number
    /// @return The sector value (0 for urban, 1 for rural)
    /// @throws HgpsException if sector is unknown
    float sector_to_value() const;

    /// @brief Gets the income enumeration as a number
    /// @return The income value (low = 1, lowermiddle = 2, uppermiddle = 3, high = 4)
    /// @throws HgpsException if income is unknown
    float income_to_value() const;

    /// @brief Gets region membership value as float
    /// @returns The region membership value (1: England, 2: Wales, 3: Scotland, 4: Northern
    /// Ireland)
    /// @throw HgpsException if region is unknown
    float region_to_value() const;

    /// @brief Gets ethnicity membership value as float
    /// @returns The ethnicity membership value (1: White, 2: Black, 3: Asian, 4: Mixed, 5: Other)
    /// @throw HgpsException if ethnicity is unknown
    float ethnicity_to_value() const;

    /// @brief Gets the region enumeration as a string
    /// @return The region name string
    /// @throws HgpsException if region is unknown
    std::string region_to_string() const;

    /// @brief Gets the ethnicity enumeration as a string
    /// @return The ethnicity name string
    /// @throws HgpsException if ethnicity is unknown
    std::string ethnicity_to_string() const;

    /// @brief Copies the data from another Person instance
    /// @param other The source Person instance
    void copy_from(const Person &other);

    /// @brief Emigrate this instance from the virtual population
    /// @param time Migration time
    /// @throws std::logic_error for attempting to set to non-active individuals.
    void emigrate(const unsigned int time);

    /// @brief Mark this instance as dead
    /// @param time Death time
    /// @throws std::logic_error for attempting to set to non-active individuals.
    void die(const unsigned int time);

    /// @brief Resets the unique identifier sequence to zero.
    static void reset_id();

  private:
    std::size_t id_{};
    bool is_alive_{true};
    bool has_emigrated_{false};
    unsigned int time_of_death_{};
    unsigned int time_of_migration_{};

    static std::atomic<std::size_t> newUID;
    static std::unordered_map<core::Identifier, std::function<double(const Person &)>>
        current_dispatcher;
};

} // namespace hgps
