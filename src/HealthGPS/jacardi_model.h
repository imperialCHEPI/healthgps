#pragma once

#include "jacardi_education_lifecycle.h"
#include "jacardi_employment_static.h"
#include "risk_factor_model.h"

#include <memory>
#include <string>

namespace hgps {

/// MAHIMA: JACARDI static / init pathway (`ModelName`: JacardiModel).
/// Opt-in ladder: Education and/or Employment. Employment runs after Education.
class JacardiModel final : public RiskFactorModel {
  public:
    JacardiModel(std::shared_ptr<const EducationLifecycleTables> education,
                 std::shared_ptr<const EmploymentStaticTables> employment);

    RiskFactorModelType type() const noexcept override;
    std::string name() const noexcept override;

    /// Part A for the full active population at simulation start.
    void generate_risk_factors(RuntimeContext &context) override;

    /// Part A only for people still missing finite Education / Employment.
    void update_risk_factors(RuntimeContext &context) override;

  private:
    void apply_education_part_a(Person &person, Random &rng) const;
    void apply_employment_part_a(Person &person, Random &rng) const;

    std::shared_ptr<const EducationLifecycleTables> education_;
    std::shared_ptr<const EmploymentStaticTables> employment_;
};

class JacardiModelDefinition final : public RiskFactorModelDefinition {
  public:
    JacardiModelDefinition(std::shared_ptr<EducationLifecycleTables> education,
                           std::shared_ptr<EmploymentStaticTables> employment);

    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::shared_ptr<EducationLifecycleTables> education_;
    std::shared_ptr<EmploymentStaticTables> employment_;
};

} // namespace hgps
