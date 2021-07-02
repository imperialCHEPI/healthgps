#pragma once

#include <iostream>
#include <memory>
#include "interfaces.h"

namespace hgps {

	class CountryModule : public SimulationModule
	{
	public:
		CountryModule() = delete;
		explicit CountryModule(std::vector<core::Country> countries, core::Country current)
			: countries_{ countries }, current_{ current }
		{
		}

		SimulationModuleType type() const override { return SimulationModuleType::Simulator; };

		std::string name() const override { return "Country"; }

		void execute(std::string command) {
			std::cout << command << ": " << current_.name << std::endl;
		}

	private:
		core::Country current_;
		std::vector<core::Country> countries_;
	};

	static std::unique_ptr<CountryModule> build_country_module(core::Datastore& manager, ModelInput& config) {
		return std::make_unique<CountryModule>(manager.get_countries(), config.settings().country());
	}
}