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
    // MAHIMA: Part A — assign education once for everyone in the start year.
    auto &rng = context.random();
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        set_person_education(person, education_->initialise(person.age, person.gender, rng));
    }
}

void JacardiModel::update_risk_factors(RuntimeContext &context) {
    // MAHIMA: Newborns / anyone without Education get Part A for their current age.
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
