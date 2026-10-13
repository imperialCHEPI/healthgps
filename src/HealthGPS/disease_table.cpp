#include "disease_table.h"

#include <fmt/format.h>
#include <stdexcept>
#include <utility>

namespace hgps {
/* --------------------   Disease Measure Implementation ----------------- */

DiseaseMeasure::DiseaseMeasure(std::map<int, double> measures) : measures_{std::move(measures)} {}

std::size_t DiseaseMeasure::size() const noexcept { return measures_.size(); }

double DiseaseMeasure::at(const int measure_id) const { return measures_.at(measure_id); }

double DiseaseMeasure::operator[](const int measure_id) const { return measures_.at(measure_id); }

/* --------------------   Disease Table Implementation ----------------- */

DiseaseTable::DiseaseTable(const core::DiseaseInfo &info, std::map<std::string, int> &&measures,
                           std::map<int, std::map<core::Gender, DiseaseMeasure>> &&data)
    : info_{info}, measures_{std::move(measures)}, data_{std::move(data)} {

    if (info.code.is_empty()) {
        throw std::invalid_argument("Invalid disease information with empty identifier");
    }
    if (data_.empty()) {
        return; // empty table
    }

    // Consistence checks, otherwise number of columns is wrong.
    auto col_size = data_.begin()->second.size();
    for (const auto &age : data_) {
        if (age.second.size() != col_size) {
            throw std::invalid_argument(
                fmt::format("Number of columns mismatch at age: {} ({} vs {}).", age.first,
                            age.second.size(), col_size));
        }
    }
}

const core::DiseaseInfo &DiseaseTable::info() const noexcept { return info_; }

std::size_t DiseaseTable::size() const noexcept { return rows() * cols(); }

std::size_t DiseaseTable::rows() const noexcept { return data_.size(); }

std::size_t DiseaseTable::cols() const noexcept {
    if (data_.empty()) {
        return 0;
    }

    return data_.begin()->second.size();
}

bool DiseaseTable::contains(const int age) const noexcept { return data_.contains(age); }

bool DiseaseTable::contains(int age, core::Gender gender) const noexcept {
    const auto age_it = data_.find(age);
    return age_it != data_.end() && age_it->second.contains(gender);
}

std::string DiseaseTable::describe_prevalence_skip_coverage() const {
    const auto code = info_.code.to_string();
    if (data_.empty()) {
        // MAHIMA: ASCII "-" only — Windows console mangles UTF-8 em dash as "ÔÇö".
        return fmt::format("{} : ERROR - empty prevalence table (check disease data files)", code);
    }

    std::size_t male_ages = 0;
    std::size_t female_ages = 0;
    for (const auto &entry : data_) {
        const auto &sexes = entry.second;
        male_ages += sexes.contains(core::Gender::male) ? 1u : 0u;
        female_ages += sexes.contains(core::Gender::female) ? 1u : 0u;
    }

    // MAHIMA: Every age row must list the same genders (constructor rule). Incomplete
    // gender coverage therefore cannot appear mid-row; it shows only as missing ages in [min, max].
    const int min_age = data_.begin()->first;
    const int max_age = data_.rbegin()->first;
    // MAHIMA: Widen before subtract (bugprone-misplaced-widening-cast); ages are non-negative.
    const auto span =
        static_cast<std::size_t>(max_age) - static_cast<std::size_t>(min_age) + std::size_t{1};
    const std::size_t missing_ages = span > data_.size() ? span - data_.size() : 0u;

    if (male_ages == 0 && female_ages > 0) {
        if (missing_ages > 0) {
            return fmt::format("{} : WARNING - female-only table but {} age(s) missing in [{}, {}] "
                               "(sex-limited OK; age gaps look like a CSV error)",
                               code, missing_ages, min_age, max_age);
        }
        return fmt::format("{} : EXPECTED - female-only table; males skipped at prevalence init "
                           "(sex-limited disease, not a missing file)",
                           code);
    }
    if (female_ages == 0 && male_ages > 0) {
        if (missing_ages > 0) {
            return fmt::format("{} : WARNING - male-only table but {} age(s) missing in [{}, {}] "
                               "(sex-limited OK; age gaps look like a CSV error)",
                               code, missing_ages, min_age, max_age);
        }
        return fmt::format("{} : EXPECTED - male-only table; females skipped at prevalence init "
                           "(sex-limited disease, not a missing file)",
                           code);
    }
    if (missing_ages > 0) {
        return fmt::format(
            "{} : WARNING - both sexes present but {} age row(s) missing in [{}, {}] "
            "(possible disease CSV gap - verify input files)",
            code, missing_ages, min_age, max_age);
    }
    return fmt::format("{} : OK - both sexes covered; contiguous ages [{}, {}] in prevalence table",
                       code, min_age, max_age);
}

const std::map<std::string, int> &DiseaseTable::measures() const noexcept { return measures_; }

int DiseaseTable::at(const std::string &measure) const { return measures_.at(measure); }

int DiseaseTable::operator[](const std::string &measure) const { return measures_.at(measure); }

DiseaseMeasure &DiseaseTable::operator()(const int age, const core::Gender gender) {
    return data_.at(age).at(gender);
}

const DiseaseMeasure &DiseaseTable::operator()(const int age, const core::Gender gender) const {
    return data_.at(age).at(gender);
}

} // namespace hgps
