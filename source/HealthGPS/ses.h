#pragma once

#include "interfaces.h"
#include "modelinput.h"

namespace hgps {

	class SESRecord
	{
	public:
		SESRecord(core::Gender gender, int age, float education, float incoming, float weight)
			: gender_{ gender }, age_{ age }, 
			  education_level_{ education }, incoming_level_{ incoming },
			  weight_{ weight } {}

		const core::Gender& gender() const noexcept { return gender_; }

		const int age() const noexcept { return age_; }

		const float& education_level() const noexcept { return education_level_; }

		const float& incoming_level() const noexcept { return incoming_level_; }

		const float& weight() const noexcept { return weight_; }

	private:
		core::Gender gender_;
		int age_;
		float education_level_{};
		float incoming_level_{};
		float weight_{};
	};

	class SESModule final : public SimulationModule
	{
	public:
		SESModule() = delete;
		SESModule(std::vector<SESRecord>&& data, core::IntegerInterval age_range);

		SimulationModuleType type() const override;

		std::string name() const override;

		const std::vector<SESRecord>& data() const noexcept;

		const int& max_education_level() const noexcept;

		const int& max_incoming_level() const noexcept;

		const std::map<int, std::vector<float>> get_education_frequency(std::optional<core::Gender> filter) const;

		const std::map<int, core::FloatArray2D> get_income_frenquency(std::optional<core::Gender> filter) const;

	private:
		std::vector<SESRecord> data_;
		core::IntegerInterval age_range_;
		int max_education_level_{};
		int max_income_level_{};

		void calculate_max_levels();
	};

	core::Gender parse_gender(const std::any& value);

	int parse_integer(const std::any& value);

	float parse_float(const std::any& value);

	std::unique_ptr<SESModule> build_ses_module(core::Datastore& manager, ModelInput& config);
}

