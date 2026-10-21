#include "jacardi_employment_static.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/identifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>
#include <string>

namespace hgps {
namespace {
// MAHIMA: Output keys — mean_EmploymentDetailed / mean_Employment when listed in risk_factors.
const auto employment_detailed_factor = core::Identifier{"EmploymentDetailed"};
const auto employment_factor = core::Identifier{"Employment"};

// MAHIMA: RCS knots t1..t7. Last two enter the formula as 66 and 80.
constexpr std::array<double, 7> k_knots{21.0, 24.0, 28.0, 43.0, 55.0, 66.0, 80.0};
constexpr double k_rcs_scale = 3481.0; // (80 - 21)^2
constexpr double k_knot_penultimate = 66.0;
constexpr double k_knot_last = 80.0;
constexpr double k_knot_span = 14.0; // 80 - 66

double pos_cube(double z) {
    const double p = (std::max)(z, 0.0);
    return p * p * p;
}

} // namespace

void EmploymentStaticTables::set_coefficients(
    std::unordered_map<int, std::vector<FactorTerm>> by_category) {
    coefficients_ = std::move(by_category);
}

void EmploymentStaticTables::set_recode(std::array<int, category_count + 1> detailed_to_cat4) {
    recode_ = detailed_to_cat4;
    recode_loaded_ = true;
}

int EmploymentStaticTables::gender_code(core::Gender gender) {
    if (gender == core::Gender::male) {
        return 1;
    }
    if (gender == core::Gender::female) {
        return 0;
    }
    throw core::HgpsException{"JACARDI employment: unknown gender"};
}

EmploymentStaticTables::AgeFactors EmploymentStaticTables::compute_age_factors(int age_capped) {
    // MAHIMA: Age RCS_j = [(a-t_j)_+^3 - (a-66)_+^3*(80-t_j)/14 + (a-80)_+^3*(66-t_j)/14] / 3481
    AgeFactors out;
    out.age = static_cast<double>(age_capped);
    out.school_fade = (std::max)(0.0, 22.0 - out.age) / 3.0;
    for (int j = 0; j < 5; ++j) {
        const double tj = k_knots[static_cast<std::size_t>(j)];
        const double term = pos_cube(out.age - tj) -
                            pos_cube(out.age - k_knot_penultimate) * (k_knot_last - tj) / k_knot_span +
                            pos_cube(out.age - k_knot_last) * (k_knot_penultimate - tj) / k_knot_span;
        out.age_rcs[static_cast<std::size_t>(j)] = term / k_rcs_scale;
    }
    return out;
}

double EmploymentStaticTables::factor_value(const std::string &factor, const AgeFactors &age_f,
                                              int gender, int education_id) {
    // MAHIMA: Product factors A*B = value(A)*value(B); recurse once on each side.
    const auto star = factor.find('*');
    if (star != std::string::npos) {
        const auto left = factor.substr(0, star);
        const auto right = factor.substr(star + 1);
        return factor_value(left, age_f, gender, education_id) *
               factor_value(right, age_f, gender, education_id);
    }

    if (factor == "Intercept") {
        return 1.0;
    }
    if (factor == "Age") {
        return age_f.age;
    }
    if (factor == "AgeRcs1") {
        return age_f.age_rcs[0];
    }
    if (factor == "AgeRcs2") {
        return age_f.age_rcs[1];
    }
    if (factor == "AgeRcs3") {
        return age_f.age_rcs[2];
    }
    if (factor == "AgeRcs4") {
        return age_f.age_rcs[3];
    }
    if (factor == "AgeRcs5") {
        return age_f.age_rcs[4];
    }
    if (factor == "SchoolFade") {
        return age_f.school_fade;
    }
    if (factor == "Gender") {
        return static_cast<double>(gender);
    }
    // MAHIMA: education_id 1..7 → ISCED dummies; ISCED 3 (id 3) is reference (no factor).
    if (factor == "EducationISCED01") {
        return education_id == 1 ? 1.0 : 0.0;
    }
    if (factor == "EducationISCED2") {
        return education_id == 2 ? 1.0 : 0.0;
    }
    if (factor == "EducationISCED5") {
        return education_id == 4 ? 1.0 : 0.0;
    }
    if (factor == "EducationISCED6") {
        return education_id == 5 ? 1.0 : 0.0;
    }
    if (factor == "EducationISCED7") {
        return education_id == 6 ? 1.0 : 0.0;
    }
    if (factor == "EducationISCED8") {
        return education_id == 7 ? 1.0 : 0.0;
    }

    throw core::HgpsException{fmt::format("JACARDI employment: unknown factor '{}'", factor)};
}

std::array<double, EmploymentStaticTables::category_count>
EmploymentStaticTables::score_probabilities(unsigned int age, core::Gender gender,
                                            int education_id) const {
    if (!has_coefficients()) {
        throw core::HgpsException{"JACARDI employment SCORE: coefficients not loaded"};
    }
    if (education_id < 1 || education_id > 7) {
        throw core::HgpsException{
            fmt::format("JACARDI employment SCORE: education_id {} out of 1..7", education_id)};
    }

    const int capped = static_cast<int>(age) > age_cap ? age_cap : static_cast<int>(age);
    const auto age_f = compute_age_factors(capped);
    const int g = gender_code(gender);

    std::array<double, category_count> scores{};
    for (int cat = category_min; cat <= category_max; ++cat) {
        double score = 0.0;
        const auto it = coefficients_.find(cat);
        if (it != coefficients_.end()) {
            for (const auto &term : it->second) {
                score += term.coefficient * factor_value(term.factor, age_f, g, education_id);
            }
        }
        scores[static_cast<std::size_t>(cat - 1)] = score;
    }

    // MAHIMA: Softmax with max-subtraction for numerical stability (same idea as income logits).
    const double max_score = *std::max_element(scores.begin(), scores.end());
    double sum_exp = 0.0;
    std::array<double, category_count> probs{};
    for (std::size_t i = 0; i < scores.size(); ++i) {
        probs[i] = std::exp(scores[i] - max_score);
        sum_exp += probs[i];
    }
    for (double &p : probs) {
        p /= sum_exp;
    }
    return probs;
}

int EmploymentStaticTables::draw_category(const std::array<double, category_count> &probs,
                                          Random &rng) {
    // MAHIMA: Inverse CDF over categories 1..6 in order (DRAW rule).
    const double u = rng.next_double();
    double running = 0.0;
    int last_positive = category_max;
    for (int cat = category_min; cat <= category_max; ++cat) {
        const double p = probs[static_cast<std::size_t>(cat - 1)];
        if (p > 0.0) {
            last_positive = cat;
        }
        running += p;
        if (running > u) {
            return cat;
        }
    }
    return last_positive;
}

int EmploymentStaticTables::recode(int employment_id) const {
    if (!recode_loaded_) {
        throw core::HgpsException{"JACARDI employment: recode table not loaded"};
    }
    if (employment_id < category_min || employment_id > category_max) {
        throw core::HgpsException{
            fmt::format("JACARDI employment recode: id {} out of 1..6", employment_id)};
    }
    return recode_[static_cast<std::size_t>(employment_id)];
}

EmploymentStaticTables::Assignment
EmploymentStaticTables::assign_draw(unsigned int age, core::Gender gender, int education_id,
                                    Random &rng) const {
    const auto probs = score_probabilities(age, gender, education_id);
    const int detailed = draw_category(probs, rng);
    return Assignment{.detailed_id = static_cast<double>(detailed),
                      .cat4_id = static_cast<double>(recode(detailed))};
}

EmploymentStaticTables::Assignment
EmploymentStaticTables::initialise(unsigned int age, core::Gender gender, int education_id,
                                    Random &rng) const {
    // MAHIMA: Part A — 0..18 missing; 19+ SCORE/DRAW/recode (age capped inside SCORE).
    if (age <= age_missing_max) {
        return Assignment{};
    }
    return assign_draw(age, gender, education_id, rng);
}

EmploymentStaticTables::Assignment
EmploymentStaticTables::update(unsigned int age, core::Gender gender, int education_id,
                               const Assignment &current, Random &rng) const {
    // MAHIMA: Part B — under 19 missing; first assign at 19+ if still missing; else hold.
    if (age <= age_missing_max) {
        return Assignment{};
    }
    if (!std::isfinite(current.detailed_id)) {
        return assign_draw(age, gender, education_id, rng);
    }
    return hold(current);
}

void set_person_employment(Person &person, const EmploymentStaticTables::Assignment &assignment) {
    person.risk_factors[employment_detailed_factor] = assignment.detailed_id;
    person.risk_factors[employment_factor] = assignment.cat4_id;
}

EmploymentStaticTables::Assignment get_person_employment(const Person &person) {
    EmploymentStaticTables::Assignment out;
    const auto d_it = person.risk_factors.find(employment_detailed_factor);
    const auto c_it = person.risk_factors.find(employment_factor);
    if (d_it != person.risk_factors.end()) {
        out.detailed_id = d_it->second;
    }
    if (c_it != person.risk_factors.end()) {
        out.cat4_id = c_it->second;
    }
    return out;
}

bool person_has_employment(const Person &person) {
    const auto it = person.risk_factors.find(employment_detailed_factor);
    return it != person.risk_factors.end() && std::isfinite(it->second);
}

} // namespace hgps
