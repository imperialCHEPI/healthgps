#include "energy_balance_model_new.h"
#include "runtime_context.h"
#include <iostream>

namespace hgps {

	EnergyBalanceHierarchicalModelNew::EnergyBalanceHierarchicalModelNew(
		EnergyBalanceModelDefinition& definition)
		: definition_{ definition } {}

	HierarchicalModelType EnergyBalanceHierarchicalModelNew::type() const noexcept {
		return HierarchicalModelType::Dynamic;
	}

	const std::string& EnergyBalanceHierarchicalModelNew::name() const noexcept {
		return name_;
	}

	void EnergyBalanceHierarchicalModelNew::generate_risk_factors([[maybe_unused]] RuntimeContext& context) {
		throw std::logic_error("EnergyBalanceHierarchicalModelNew::generate_risk_factors not yet implemented.");
	}

	void EnergyBalanceHierarchicalModelNew::update_risk_factors(RuntimeContext& context) {
		auto age_key = core::Identifier{ "age" };
		for (auto& entity : context.population()) {
			// Ignore if inactive, newborn risk factors must be generated, not updated!
			if (!entity.is_active() || entity.age == 0) {
				continue;
			}
		}
	}
}
