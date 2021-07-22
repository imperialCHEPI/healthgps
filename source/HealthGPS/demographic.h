#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "runtime_context.h"

namespace hgps {

	struct AgeRecord {
		AgeRecord(int pop_age, float males, float females)
			: age{ pop_age }, num_males{ males }, num_females{ females }
		{}

		const int age{};
		const float num_males{};
		const float num_females{};

		const float total() const noexcept { return num_males + num_females; }
	};

	struct GenderPair {
		GenderPair(double male_value, double female_value) 
			: male{ male_value }, female{ female_value }
		{}

		const double male;

		const double female;
	};

	class DemographicModule final : public SimulationModule {
	public:
		DemographicModule() = delete;
		DemographicModule(std::map<int, std::map<int, AgeRecord>>&& data);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		size_t get_total_population(const int time_year) const noexcept;

		std::map<int, GenderPair> get_age_gender_distribution(const int time_year) const noexcept;

		void initialise_population(RuntimeContext& context, const int time_year);

	private:
		std::map<int, std::map<int, AgeRecord>> data_;

		// Data limits cache 
		core::IntegerInterval time_range_{};
		core::IntegerInterval age_range_{};
	};

	std::unique_ptr<DemographicModule> build_demographic_module(core::Datastore& manager, ModelInput& config);
}

