#include "jacardi_model.h"

#include "HealthGPS.Core/exception.h"
#include "runtime_context.h"

namespace hgps {

JacardiModel::JacardiModel(std::shared_ptr<const EducationLifecycleTables> education)
    : education_{std::move(education)} {
    if (!education_ || !education_->has_lookup()) {
        throw core::HgpsException{"JacardiModel requires education lookup tables"};
    }
}

RiskFactorModelType JacardiModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string JacardiModel::name() const noexcept { return "JacardiModel"; }

void JacardiModel::generate_risk_factors(RuntimeContext &context) {
    // MAHIMA: Part A for everyone at simulation start (serial: shared Random is not thread-safe;
    // work per person is tiny vs disease/analysis, so parallelising is not worth it yet).
    auto &rng = context.random();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        set_person_education(person, education_->initialise(person.age, person.gender, rng));
    }
}

void JacardiModel::update_risk_factors(RuntimeContext &context) {
    // MAHIMA: Fill Education only when still missing (newborns / ages that stayed NaN).
    // People who already have a finite id are left alone — Part B owns yearly changes.
    auto &rng = context.random();
    for (auto &person : context.population()) {
        if (!person.is_active() || person_has_education(person)) {
            continue;
        }
        set_person_education(person, education_->initialise(person.age, person.gender, rng));
    }
}

JacardiModelDefinition::JacardiModelDefinition(std::shared_ptr<EducationLifecycleTables> education)
    : education_{std::move(education)} {}

std::unique_ptr<RiskFactorModel> JacardiModelDefinition::create_model() const {
    return std::make_unique<JacardiModel>(education_);
}

} // namespace hgps
