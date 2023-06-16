#include "energy_balance_model.h"
#include "runtime_context.h"
#include <iostream>

namespace hgps {

	EnergyBalanceModel::EnergyBalanceModel(
		EnergyBalanceModelDefinition& definition)
		: definition_{ definition } {}

	HierarchicalModelType EnergyBalanceModel::type() const noexcept {
		return HierarchicalModelType::Dynamic;
	}

	const std::string& EnergyBalanceModel::name() const noexcept {
		return name_;
	}

	void EnergyBalanceModel::generate_risk_factors([[maybe_unused]] RuntimeContext& context) {
		throw std::logic_error("EnergyBalanceModel::generate_risk_factors not yet implemented.");
	}

	void EnergyBalanceModel::update_risk_factors(RuntimeContext& context) {
		auto age_key = core::Identifier{ "age" };
		for (auto& entity : context.population()) {
			// Ignore if inactive, newborn risk factors must be generated, not updated!
			if (!entity.is_active() || entity.age == 0) {
				continue;
			}
		}
	}
}
