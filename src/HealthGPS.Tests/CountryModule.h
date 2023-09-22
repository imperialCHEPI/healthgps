#pragma once
#include "HealthGPS.Datastore/api.h"
#include "HealthGPS/api.h"

#include <iostream>
#include <memory>

class CountryModule : public hgps::SimulationModule {
  public:
    CountryModule() = delete;
    explicit CountryModule(std::vector<hgps::core::Country> countries, hgps::core::Country current)
        : countries_{countries}, current_{current} {}

    hgps::SimulationModuleType type() const noexcept override {
        return hgps::SimulationModuleType::Analysis;
    };

    const std::string &name() const noexcept override { return name_; }

    void initialise_population(hgps::RuntimeContext &) override {
        std::cout << "There are: " << countries_.size() << " countries, current: " << current_.name
                  << std::endl;
    }

    void execute(std::string command) {
        std::cout << command << ": " << current_.name << std::endl;
    }

  private:
    std::vector<hgps::core::Country> countries_;
    hgps::core::Country current_;
    std::string name_{"Country"};
};

static std::unique_ptr<CountryModule> build_country_module(hgps::Repository &repository,
                                                           const hgps::ModelInput &config) {

    return std::make_unique<CountryModule>(repository.manager().get_countries(),
                                           config.settings().country());
}
