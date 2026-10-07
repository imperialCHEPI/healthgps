#include "jacardi_model_update.h"

#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

namespace hgps {

JacardiModelUpdate::JacardiModelUpdate(std::shared_ptr<const EducationLifecycleTables> education)
    : education_{std::move(education)} {
    if (!education_ || !education_->has_draw_at_22() || !education_->has_upgrades()) {
        throw core::HgpsException{
            "JacardiModelUpdate requires draw_at_22 and upgrade education tables"};
    }
}

RiskFactorModelType JacardiModelUpdate::type() const noexcept {
    return RiskFactorModelType::Dynamic;
}

std::string JacardiModelUpdate::name() const noexcept { return "JacardiModelUpdate"; }

void JacardiModelUpdate::generate_risk_factors(RuntimeContext & /*context*/) {
    // MAHIMA: Education init lives in JacardiModel (static slot).
}

void JacardiModelUpdate::update_risk_factors(RuntimeContext &context) {
    // MAHIMA: Part B — run after DemographicModule has aged everyone by one year.
    auto &rng = context.random();
    const int year = context.time_now();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        const double current = get_person_education(person);
        const double next = education_->update(person.age, person.gender, year, current, rng);
        set_person_education(person, next);
    }
}

JacardiModelUpdateDefinition::JacardiModelUpdateDefinition(
    std::shared_ptr<EducationLifecycleTables> education)
    : education_{std::move(education)} {}

std::unique_ptr<RiskFactorModel> JacardiModelUpdateDefinition::create_model() const {
    return std::make_unique<JacardiModelUpdate>(education_);
}

} // namespace hgps
