#include "ses.h"
#include <algorithm>

namespace hgps {

	hgps::SESModule::SESModule(std::vector<SESRecord>&& data, core::IntegerInterval age_range)
		: data_{ data }, age_range_{ age_range }
	{
		calculate_max_levels();
	}

	SimulationModuleType hgps::SESModule::type() const {
		return SimulationModuleType::SES;
	}

	std::string hgps::SESModule::name() const {
		return "SES";
	}

	const std::vector<SESRecord>& hgps::SESModule::data() const noexcept {
		return data_;
	}

	const int& hgps::SESModule::max_education_level() const noexcept {
		return max_education_level_;
	}

	const int& hgps::SESModule::max_incoming_level() const noexcept {
		return max_income_level_;
	}

	void hgps::SESModule::execute(std::string_view command,
		RandomBitGenerator& generator, std::vector<Entity>& entities) {
	}

	void hgps::SESModule::execute(std::string_view command,
		RandomBitGenerator& generator, Entity& entity) {
	}

	void hgps::SESModule::calculate_max_levels() {

		for (auto& datum : data_) {
			auto inc_level = datum.incoming_level();
			auto edu_level = datum.education_level();

			if (!std::isnan(inc_level) && max_income_level_ < inc_level) {
				max_income_level_ = (int)inc_level;
			}

			if (!std::isnan(edu_level) && max_education_level_ < edu_level) {
				max_education_level_ = (int)edu_level;
			}
		}
	}

	core::Gender parse_gender(const std::any& value)
	{
		if (!value.has_value()) {
			return core::Gender::unknown;
		}

		if (const int* i = std::any_cast<int>(&value)) {
			return *i == 1 ? core::Gender::male : core::Gender::female;
		}
		else if (const std::string* s = std::any_cast<std::string>(&value))
		{
			if (s->length() == 1) {
				if (core::case_insensitive::equals(*s, "m")) {
					return core::Gender::male;
				}
				else {
					return core::Gender::female;
				}
			}

			if (core::case_insensitive::equals(*s, "male")) {
				return core::Gender::male;
			}
			else {
				return core::Gender::female;
			}
		}

		return core::Gender::unknown;
	}

	core::IntegerInterval parse_age_group(const std::any& value)
	{
		if (auto* s = std::any_cast<std::string>(&value)) {
			return  core::parse_integer_interval(*s);
		}

		return core::IntegerInterval(0,0);
	}

	float parse_float(const std::any& value)
	{
		if (value.has_value()) {
			// Using pointer to avoid exception
			if (auto* f = std::any_cast<float>(&value)) {
				return *f;
			}
			else if (auto* d = std::any_cast<double>(&value)) {
				return (float)*d; // down-cast
			}
			else if (auto* i = std::any_cast<int>(&value)) {
				return (float)*i;
			}
			else if (auto* s = std::any_cast<std::string>(&value)) {
				return std::stof(*s);
			}
		}

		return std::nanf("");
	}

	std::unique_ptr<SESModule> hgps::build_ses_module(core::Datastore& manager, ModelContext& context) {

		// SES required data, assuming it has been validated.
		auto& table = context.data();

		auto& gender_col = table.column(context.ses_mapping().entries["gender"]);
		auto& age_col = table.column(context.ses_mapping().entries["age_group"]);
		auto& edu_col = table.column(context.ses_mapping().entries["education"]);
		auto& inc_col = table.column(context.ses_mapping().entries["income"]);

		std::vector<SESRecord> data;
		for (size_t row = 0; row < table.num_rows(); row++)
		{
			auto gender = parse_gender(gender_col->value(row));
			auto age_group = parse_age_group(age_col->value(row));
			auto edu_value = parse_float(edu_col->value(row));
			float income = parse_float(inc_col->value(row));
			auto weight = 1.0f;

			data.emplace_back(SESRecord(gender, age_group, edu_value, income, weight));
		}

		return std::make_unique<SESModule>(std::move(data), context.population().age_range());
	}
}