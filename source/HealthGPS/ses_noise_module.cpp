#include "ses_noise_module.h"
#include "runtime_context.h"
#include "HealthGPS.Core/string_util.h"

namespace hgps {
	SESNoiseModule::SESNoiseModule()
		: SESNoiseModule(std::vector<double>{0.0, 1.0}) {}

	SESNoiseModule::SESNoiseModule(std::vector<double> parameters)
		: SESNoiseModule("normal", parameters) {}

	SESNoiseModule::SESNoiseModule(std::string function, std::vector<double> parameters)
		: function_{ function }, parameters_{ parameters }
	{
		if (!core::case_insensitive::equals("normal", function)) {
			throw std::invalid_argument(
				std::format("Noise generation function: {} is not supported", function));
		}

		if (parameters.size() != 2) {
			throw std::invalid_argument(
				std::format("Number of parameters mismatch: expected 2, received {}", parameters.size()));
		}
	}

	SimulationModuleType SESNoiseModule::type() const noexcept {
		return SimulationModuleType::SES;
	}

	std::string SESNoiseModule::name() const noexcept {
		return "SES";
	}

	void SESNoiseModule::initialise_population(RuntimeContext& context) {
		for (auto& entity : context.population()) {
			entity.ses = context.random().next_normal(parameters_[0], parameters_[1]);
		}
	}

	void SESNoiseModule::update_population(RuntimeContext& context) {
		auto newborn_age = 0u;
		for (auto& entity : context.population()) {
			if (entity.age > newborn_age) {
				continue;
			}

			entity.ses = context.random().next_normal(parameters_[0], parameters_[1]);
		}
	}

	std::unique_ptr<SESNoiseModule> build_ses_noise_module(Repository& repository, const ModelInput& config)
	{
		// TODO: Expose parameters in configuration
		return std::make_unique<SESNoiseModule>();
	}
}