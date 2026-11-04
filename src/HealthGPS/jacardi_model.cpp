#include "jacardi_model.h"

#include "HealthGPS.Core/exception.h"
#include "population.h"
#include "runtime_context.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fmt/format.h>
#include <string_view>

namespace hgps {
namespace {

constexpr int k_sample_bands = 4;

int sample_band(unsigned int age) noexcept {
    if (age <= EmploymentStaticTables::age_missing_max) {
        return 0;
    }
    if (age <= 30U) {
        return 1;
    }
    if (age <= 64U) {
        return 2;
    }
    return 3;
}

const char *sex_name(core::Gender gender) noexcept {
    if (gender == core::Gender::male) {
        return "male";
    }
    if (gender == core::Gender::female) {
        return "female";
    }
    return "unknown";
}

std::string id_or_missing(double value) {
    if (!std::isfinite(value)) {
        return "missing";
    }
    return std::to_string(static_cast<int>(value));
}

constexpr std::array<std::string_view, k_sample_bands> k_band_name{"0-18", "19-30", "31-64", "65+"};
constexpr int k_education_ids = 7;
constexpr int k_cat4_ids = 4;

// MAHIMA: Fixed bins so one pass counts missing, legal ids, and anything outside the range.
template <int N> struct CategoryTally {
    int missing{0};
    std::array<int, N> bins{};
    int other{0};
};

struct BandSexCount {
    int n{0};
    CategoryTally<k_education_ids> education{};
    CategoryTally<EmploymentStaticTables::category_count> detailed{};
    CategoryTally<k_cat4_ids> cat4{};
};

template <int N> void add_category(CategoryTally<N> &tally, double value) noexcept {
    if (!std::isfinite(value)) {
        ++tally.missing;
        return;
    }
    const int id = static_cast<int>(value);
    if (id < 1 || id > N) {
        ++tally.other;
        return;
    }
    ++tally.bins[static_cast<std::size_t>(id - 1)];
}

std::string count_cell(int count, int n) {
    const double pct = n == 0 ? 0.0 : (100.0 * static_cast<double>(count) / static_cast<double>(n));
    return fmt::format("{} ({:.1f}%)", count, pct);
}

template <int N>
void append_share_table(std::string &out, std::string_view title,
                        const std::array<std::array<BandSexCount, 2>, k_sample_bands> &counts,
                        const CategoryTally<N> BandSexCount::*field) {
    out += title;
    out += '\n';
    out += fmt::format("{:<6} | {:<6} | {:>5} | {:>12}", "band", "sex", "n", "missing");
    for (int id = 1; id <= N; ++id) {
        out += fmt::format(" | {:>12}", id);
    }
    out += fmt::format(" | {:>12}\n", "other");

    for (int band = 0; band < k_sample_bands; ++band) {
        for (int sex = 0; sex < 2; ++sex) {
            const BandSexCount &row =
                counts[static_cast<std::size_t>(band)][static_cast<std::size_t>(sex)];
            if (row.n == 0) {
                continue;
            }
            const CategoryTally<N> &tally = row.*field;
            const char *sex_label = sex_name(core::Gender::female);
            if (sex == 0) {
                sex_label = sex_name(core::Gender::male);
            }
            out += fmt::format("{:<6} | {:<6} | {:>5} | {:>12}",
                               k_band_name[static_cast<std::size_t>(band)], sex_label, row.n,
                               count_cell(tally.missing, row.n));
            for (int id = 0; id < N; ++id) {
                out += fmt::format(" | {:>12}",
                                   count_cell(tally.bins[static_cast<std::size_t>(id)], row.n));
            }
            out += fmt::format(" | {:>12}\n", count_cell(tally.other, row.n));
        }
    }
}

} // namespace

std::string format_education_employment_sample(const Population &population) {
    // MAHIMA: One pass: eight sample slots plus age-band x sex counts. O(n) time, fixed bins.
    const Person *slots[k_sample_bands][2]{};
    std::array<std::array<BandSexCount, 2>, k_sample_bands> counts{};
    int unknown_gender = 0;
    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }
        int sex = -1;
        if (person.gender == core::Gender::male) {
            sex = 0;
        } else if (person.gender == core::Gender::female) {
            sex = 1;
        }
        if (sex < 0) {
            ++unknown_gender;
            continue;
        }
        const int band = sample_band(person.age);
        if (slots[band][sex] == nullptr) {
            slots[band][sex] = &person;
        }
        BandSexCount &row = counts[static_cast<std::size_t>(band)][static_cast<std::size_t>(sex)];
        ++row.n;
        add_category(row.education, get_person_education(person));
        const auto employment = get_person_employment(person);
        add_category(row.detailed, employment.detailed_id);
        add_category(row.cat4, employment.cat4_id);
    }

    std::string out;
    out += "DEBUG local (do not merge) - Education / Employment after JacardiModel Part A "
           "(start year only)\n";
    out += "ages 0-18 should be missing; 19+ should have Education 1-7, "
           "EmploymentDetailed 1-6, Employment 1-4\n";
    out += fmt::format("{:>6} {:>4} {:<8} {:>11} {:>14} {:>12}\n", "id", "age", "sex", "education",
                       "emp_detailed", "employment");
    for (int band = 0; band < k_sample_bands; ++band) {
        for (int sex = 0; sex < 2; ++sex) {
            const Person *person = slots[band][sex];
            if (person == nullptr) {
                continue;
            }
            const auto employment = get_person_employment(*person);
            out += fmt::format(
                "{:>6} {:>4} {:<8} {:>11} {:>14} {:>12}\n", person->id(), person->age,
                sex_name(person->gender), id_or_missing(get_person_education(*person)),
                id_or_missing(employment.detailed_id), id_or_missing(employment.cat4_id));
        }
    }
    out += "counts by age band and sex. Percent is of people in that band and sex.\n";
    out += "other = finite id outside the legal range. Ages 0-18 should be 100% missing. "
           "Ages 19+ should be 0% missing and 0% other.\n";
    append_share_table(out, "Education counts", counts, &BandSexCount::education);
    append_share_table(out, "EmploymentDetailed counts", counts, &BandSexCount::detailed);
    append_share_table(out, "Employment counts", counts, &BandSexCount::cat4);
    if (unknown_gender > 0) {
        out += fmt::format("skipped unknown gender: {}\n", unknown_gender);
    }
    return out;
}

JacardiModel::JacardiModel(std::shared_ptr<const EducationLifecycleTables> education,
                           std::shared_ptr<const EmploymentStaticTables> employment)
    : education_{std::move(education)}, employment_{std::move(employment)} {
    // MAHIMA: At least one ladder step must be configured (schema is opt-in per step).
    const bool has_edu = education_ && education_->has_lookup();
    const bool has_emp =
        employment_ && employment_->has_coefficients() && employment_->has_recode();
    if (!has_edu && !has_emp) {
        throw core::HgpsException{
            "JacardiModel requires Education.lookup and/or Employment coefficients+recode"};
    }
}

RiskFactorModelType JacardiModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string JacardiModel::name() const noexcept { return "JacardiModel"; }

void JacardiModel::apply_education_part_a(Person &person, Random &rng) const {
    if (!education_ || !education_->has_lookup()) {
        return;
    }
    set_person_education(person, education_->initialise(person.age, person.gender, rng));
}

void JacardiModel::apply_employment_part_a(Person &person, Random &rng) const {
    if (!employment_ || !employment_->has_coefficients()) {
        return;
    }
    // MAHIMA: Employment needs education_id at 19+; under 19 stays missing without reading edu.
    int education_id = 0;
    if (person.age > EmploymentStaticTables::age_missing_max) {
        if (!person_has_education(person)) {
            throw core::HgpsException{
                "JacardiModel employment Part A: person 19+ missing Education"};
        }
        education_id = static_cast<int>(get_person_education(person));
    }
    set_person_employment(person,
                          employment_->initialise(person.age, person.gender, education_id, rng));
}

void JacardiModel::generate_risk_factors(RuntimeContext &context) {
    // MAHIMA: Part A for everyone at start (serial: shared Random is not thread-safe).
    auto &rng = context.random();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        apply_education_part_a(person, rng);
        apply_employment_part_a(person, rng);
    }
    // MAHIMA: Local screen check only. Yearly Part B is intentionally not printed.
    fmt::print("{}", format_education_employment_sample(context.population()));
    std::fflush(stdout);
}

void JacardiModel::update_risk_factors(RuntimeContext &context) {
    // MAHIMA: Fill only still-missing Education / Employment (newborns / ages that stayed NaN).
    auto &rng = context.random();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        if (education_ && education_->has_lookup() && !person_has_education(person)) {
            apply_education_part_a(person, rng);
        }
        if (employment_ && employment_->has_coefficients() && !person_has_employment(person)) {
            apply_employment_part_a(person, rng);
        }
    }
}

JacardiModelDefinition::JacardiModelDefinition(std::shared_ptr<EducationLifecycleTables> education,
                                               std::shared_ptr<EmploymentStaticTables> employment)
    : education_{std::move(education)}, employment_{std::move(employment)} {}

std::unique_ptr<RiskFactorModel> JacardiModelDefinition::create_model() const {
    return std::make_unique<JacardiModel>(education_, employment_);
}

} // namespace hgps
