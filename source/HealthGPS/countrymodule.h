#pragma once

#include <iostream>
#include <memory>
#include "interfaces.h"

namespace hgps {

	class CountryModule : public Module
	{
	public:
		CountryModule() = delete;
		explicit CountryModule(std::vector<hgps::core::Country> countries)
			: countries_{ countries }
		{
		}

		ModuleType type() const override { return ModuleType::Simulator; };

		std::string name() const override { return "country"; }

		virtual void execute(std::string_view command, std::vector<Entity>& entities) override {
			for (auto& entity : entities) {
				execute(command, entity);
			}
		}

		void execute(std::string_view command, Entity& entity) override {
			std::cout << command << ": " << entity.to_string() << " to " << name() << std::endl;
		}

	private:

		std::vector<hgps::core::Country> countries_;
	};

	static std::unique_ptr<CountryModule> build_country_module(hgps::core::Datastore& manager) {
		return std::make_unique<CountryModule>(manager.get_countries());
	}
}