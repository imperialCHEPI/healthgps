#pragma once

#include <iostream>
#include <memory>

#include "HealthGPS/api.h"
#include "HealthGPS.Datastore\api.h"

class CountryModule : public hgps::SimulationModule
{
public:
	CountryModule() = delete;
	explicit CountryModule(std::vector<hgps::core::Country> countries, hgps::core::Country current)
		: countries_{ countries }, current_{ current }
	{
	}

	hgps::SimulationModuleType type() const noexcept override {
		return hgps::SimulationModuleType::Analysis;
	};

	std::string name() const noexcept override { return "Country"; }

	void initialise_population(hgps::RuntimeContext& context) override {
		std::cout << "There are: " << countries_.size() <<
			" countries, current: " << current_.name << std::endl;
	}

	void execute(std::string command) {
		std::cout << command << ": " << current_.name << std::endl;
	}

private:
	hgps::core::Country current_;
	std::vector<hgps::core::Country> countries_;
};

static std::unique_ptr<CountryModule> build_country_module(
	hgps::Repository& repository, const hgps::ModelInput& config) {

	return std::make_unique<CountryModule>(
		repository.manager().get_countries(), config.settings().country());
}