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

		SimulationModuleType type() const noexcept override {
			return SimulationModuleType::Simulator;
		};

		std::string name() const noexcept override { return "Country"; }

		void initialise_population(RuntimeContext& context) override {
			std::cout << "There are: " << countries_.size() << 
				" countries, current: " << current_.name << std::endl;
		}

		void execute(std::string command) {
			std::cout << command << ": " << current_.name << std::endl;
		}

	private:
		core::Country current_;
		std::vector<core::Country> countries_;
	};

	static std::unique_ptr<CountryModule> build_country_module(core::Datastore& manager, const ModelInput& config) {
		return std::make_unique<CountryModule>(manager.get_countries(), config.settings().country());
	}
}