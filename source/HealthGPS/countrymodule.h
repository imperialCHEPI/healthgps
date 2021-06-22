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

		std::string name() const override { return "country"; }

		virtual void execute(std::string_view command, RandomBitGenerator& generator, std::vector<Entity>& entities) override {
			for (auto& entity : entities) {
				execute(command, generator, entity);
			}
		}

		void execute(std::string_view command, RandomBitGenerator& generator, Entity& entity) override {
			std::cout << command << ": " << entity.to_string() << " from " <<
				current_.name << " lucky #: " << generator() << std::endl;
		}

	private:
		core::Country current_;
		std::vector<core::Country> countries_;
	};

	static std::unique_ptr<CountryModule> build_country_module(core::Datastore& manager, ModelContext& context) {
		return std::make_unique<CountryModule>(manager.get_countries(), context.population().country());
	}
}