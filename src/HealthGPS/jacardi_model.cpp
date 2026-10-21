#include "jacardi_model.h"

#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

namespace hgps {

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
