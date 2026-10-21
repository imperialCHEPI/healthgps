#include "jacardi_model_update.h"

#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

namespace hgps {

JacardiModelUpdate::JacardiModelUpdate(std::shared_ptr<const EducationLifecycleTables> education,
                                       std::shared_ptr<const EmploymentStaticTables> employment)
    : education_{std::move(education)}, employment_{std::move(employment)} {
    const bool has_edu =
        education_ && education_->has_draw_at_19() && education_->has_upgrades();
    const bool has_emp =
        employment_ && employment_->has_coefficients() && employment_->has_recode();
    if (!has_edu && !has_emp) {
        throw core::HgpsException{
            "JacardiModelUpdate requires Education Part B tables and/or Employment "
            "coefficients+recode"};
    }
}

RiskFactorModelType JacardiModelUpdate::type() const noexcept {
    return RiskFactorModelType::Dynamic;
}

std::string JacardiModelUpdate::name() const noexcept { return "JacardiModelUpdate"; }

void JacardiModelUpdate::generate_risk_factors(RuntimeContext & /*context*/) {
    // MAHIMA: Start-year assignment is owned by JacardiModel (static slot). No-op here.
}

void JacardiModelUpdate::update_risk_factors(RuntimeContext &context) {
    // MAHIMA: Part B after ageing — education first, then employment (needs education_id).
    auto &rng = context.random();
    const int year = context.time_now();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (education_ && education_->has_draw_at_19() && education_->has_upgrades()) {
            const double current = get_person_education(person);
            const double next = education_->update(person.age, person.gender, year, current, rng);
            set_person_education(person, next);
        }

        if (employment_ && employment_->has_coefficients()) {
            int education_id = 0;
            if (person.age > EmploymentStaticTables::age_missing_max) {
                if (!person_has_education(person)) {
                    throw core::HgpsException{
                        "JacardiModelUpdate employment: person 19+ missing Education"};
                }
                education_id = static_cast<int>(get_person_education(person));
            }
            const auto current = get_person_employment(person);
            const auto next =
                employment_->update(person.age, person.gender, education_id, current, rng);
            set_person_employment(person, next);
        }
    }
}

JacardiModelUpdateDefinition::JacardiModelUpdateDefinition(
    std::shared_ptr<EducationLifecycleTables> education,
    std::shared_ptr<EmploymentStaticTables> employment)
    : education_{std::move(education)}, employment_{std::move(employment)} {}

std::unique_ptr<RiskFactorModel> JacardiModelUpdateDefinition::create_model() const {
    return std::make_unique<JacardiModelUpdate>(education_, employment_);
}

} // namespace hgps
