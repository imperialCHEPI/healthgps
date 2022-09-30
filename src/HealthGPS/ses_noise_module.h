#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"

namespace hgps {

	class SESNoiseModule final : public UpdatableModule
	{
	public:
		SESNoiseModule();
		SESNoiseModule(const std::vector<double>& parameters);
		SESNoiseModule(std::string function, const std::vector<double>& parameters);

		SimulationModuleType type() const noexcept override;

		const std::string& name() const noexcept override;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context);

	private:
		std::string function_;
		std::vector<double> parameters_;
		std::string name_{ "SES" };
	};

	std::unique_ptr<SESNoiseModule> build_ses_noise_module(Repository& repository, const ModelInput& config);
}
