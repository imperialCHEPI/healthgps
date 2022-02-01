#include "ses_noise_module.h"
#include "runtime_context.h"
#include "HealthGPS.Core/string_util.h"

#include <fmt/format.h>

namespace hgps {
	SESNoiseModule::SESNoiseModule()
		: SESNoiseModule(std::vector<double>{0.0, 1.0}) {}

	SESNoiseModule::SESNoiseModule(const std::vector<double>& parameters)
		: SESNoiseModule("normal", parameters) {}

	SESNoiseModule::SESNoiseModule(std::string function, const std::vector<double>& parameters)
		: function_{ function }, parameters_{ parameters }
	{
		if (!core::case_insensitive::equals("normal", function)) {
			throw std::invalid_argument(
				fmt::format("Noise generation function: {} is not supported", function));
		}

		if (parameters.size() != 2) {
			throw std::invalid_argument(
				fmt::format("Number of parameters mismatch: expected 2, received {}", parameters.size()));
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

	std::unique_ptr<SESNoiseModule> build_ses_noise_module([[maybe_unused]] Repository& repository, const ModelInput& config)
	{
		const auto& ses = config.ses_definition();
		return std::make_unique<SESNoiseModule>(ses.fuction_name, ses.parameters);
	}
}