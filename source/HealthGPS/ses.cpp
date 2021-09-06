#include "ses.h"
#include <algorithm>
#include <cassert>
#include <numeric>

#include "converter.h"

namespace hgps {

	hgps::SESModule::SESModule(std::vector<SESRecord>&& data, core::IntegerInterval age_range)
		: data_{ data }, age_range_{ age_range }
	{
		calculate_max_levels();
	}

	SimulationModuleType hgps::SESModule::type() const noexcept {
		return SimulationModuleType::SES;
	}

	std::string hgps::SESModule::name() const noexcept {
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

	const std::map<int, std::vector<float>> SESModule::get_education_frequency(
		std::optional<core::Gender> filter) const {

		auto results = std::map<int, std::vector<float>>();
		if (data_.size() < 1) {
			return results;
		}
		
		auto data_sz = max_education_level_ + 1;
		for (auto& item : data_) {
			if (!results.contains(item.age())) { 
				results.emplace(item.age(), std::vector<float>(data_sz));
				results.at(item.age()).reserve(data_sz);
			}

			if (item.education_level() >= 0 && 
				(!filter.has_value() || item.gender() == filter.value())) {
				results.at(item.age())[(int)item.education_level()] += item.weight();
			}
		}

		// Fill gaps in data
		for (auto age = age_range_.lower(); age <= age_range_.upper(); age++) {
			if (!results.contains(age)) {
				results.emplace(age, std::vector<float>(data_sz, 1.0f));
			}
		}

		return results;
	}

	const std::map<int, core::FloatArray2D> SESModule::get_income_frenquency(
		std::optional<core::Gender> filter) const {

		auto results = std::map<int, core::FloatArray2D>();
		if (data_.size() < 1) {
			return results;
		}

		int num_rows = max_education_level_ + 1;
		int num_cols = max_income_level_ + 1;
		for (auto& r : data_)
		{
			if (isnan(r.incoming_level() + r.education_level())) {
				continue;
			}

			if (!results.contains(r.age())) {
				results.emplace(r.age(), core::FloatArray2D(num_rows, num_cols));
			}

			if (!filter.has_value() || r.gender() == filter.value()) {
				results[r.age()]((int)r.education_level(), (int)r.incoming_level()) += r.weight();
			}
		}

		// Fill gaps in data.
		for (auto age = age_range_.lower(); age <= age_range_.upper(); age++) {
			if (!results.contains(age)) {
				results.emplace(age, core::FloatArray2D(num_rows, num_cols, 1.0f));
			}
		}

		return results;
	}

	void SESModule::initialise_population(RuntimeContext& context) {
		
		// Should this calculation be cached?
		auto edu_male = get_education_frequency(core::Gender::male);
		auto edu_female = get_education_frequency(core::Gender::female);

		auto income_male = get_income_frenquency(core::Gender::male);
		auto income_female = get_income_frenquency(core::Gender::female);

		for (auto& entity : context.population()) {
			assert(entity.gender != core::Gender::unknown);
			// Note that order is important
			if (entity.gender == core::Gender::male) {
				entity.education.set_both_values(sample_education(context, edu_male.at(entity.age)));
				entity.income.set_both_values(sample_income(context, entity.education.value(), income_male.at(entity.age)));
			}
			else {
				entity.education.set_both_values(sample_education(context, edu_female.at(entity.age)));
				entity.income.set_both_values(sample_income(context, entity.education.value(), income_female.at(entity.age)));
			}
		}
	}

	void SESModule::update_population(RuntimeContext& context) {

		// Education
		auto male_education_table = get_education_frequency(core::Gender::male);
		auto female_education_table = get_education_frequency(core::Gender::female);
		auto education_levels = std::vector<int>(male_education_table.begin()->second.size());
		std::iota(education_levels.begin(), education_levels.end(), 0);

		// Income
		auto male_income_table = get_income_frenquency(core::Gender::male);
		auto female_income_table = get_income_frenquency(core::Gender::female);
		auto number_of_levels = male_income_table.begin()->second.columns();
		auto income_levels = std::vector<int>(number_of_levels);
		std::iota(income_levels.begin(), income_levels.end(), 0);

		auto education_freq = std::vector<float>{};
		auto income_freq = std::vector<float>{};
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.gender == core::Gender::male) {
				education_freq = male_education_table.at(entity.age);
				income_freq = std::vector<float>(number_of_levels);
				for (int level = 0; level < number_of_levels; level++) {
					income_freq[level] = male_income_table.at(entity.age)(entity.education.value(), level);
				}
			}
			else {
				education_freq = female_education_table.at(entity.age);
				income_freq = std::vector<float>(number_of_levels);
				for (int level = 0; level < number_of_levels; level++) {
					income_freq[level] = female_income_table.at(entity.age)(entity.education.value(), level);
				}
			}

			update_education_level(context, entity, education_levels, education_freq);
			update_income_level(context, entity, income_levels, income_freq);
		}
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

	int SESModule::sample_education(RuntimeContext& context,
		const std::vector<float>& edu_values) {

		auto total_sum = std::accumulate(edu_values.begin(), edu_values.end(), 0.0);
		auto prob_sample = context.next_double();

		auto cdf_sum = 0.0;
		auto pdf_prob = 0.0;
		auto pdf_default = 1.0 / static_cast<double>(edu_values.size() - 1);
		for (auto idx = 0; idx < edu_values.size(); idx++) {
			pdf_prob = total_sum > 0.0 ? edu_values[idx] / total_sum : pdf_default;

			cdf_sum += pdf_prob;
			if (cdf_sum >= prob_sample) {
				return idx;
			}
		}

		return -1;
	}

	int SESModule::sample_income(RuntimeContext& context,
		const int education, core::FloatArray2D& income_values) {

		auto total_sum = 0.0;
		for (size_t i = 0; i < income_values.columns(); i++) {
			total_sum += income_values(education, i);
		}

		auto prob_sample = context.next_double();

		auto cdf_sum = 0.0;
		auto pdf_prob = 0.0;
		auto pdf_default = 1.0 / static_cast<double>(income_values.columns() - 1);
		for (auto col = 0; col < income_values.columns(); col++) {
			pdf_prob = pdf_default;
			if (total_sum > 0.0) {
				pdf_prob = income_values(education, col) / total_sum;
			}

			cdf_sum += pdf_prob;
			if (cdf_sum >= prob_sample) {
				return col;
			}
		}

		return -1;
	}

	void SESModule::update_education_level(RuntimeContext& context, Person& entity,
		std::vector<int>& education_levels, std::vector<float>& education_freq)
	{
		auto education_cdf = detail::create_cdf(education_freq);
		auto random_education_level = context.next_empirical_discrete(education_levels, education_cdf);

		// Very important to retrieve the old education level when calculating
		// the risk value from the previous year.
		if (entity.education.value() == 0) {
			entity.education.set_both_values(random_education_level);
		}
		else {
			entity.education = entity.education.value();

			// To prevent education level getting maximal values ???
			if (entity.age < 31) {
				entity.education = std::max(entity.education.value(), random_education_level);
			}
		}
	}

	void SESModule::update_income_level(RuntimeContext& context, Person& entity,
		std::vector<int>& income_levels, std::vector<float>& income_freq)
	{
		auto income_cdf = detail::create_cdf(income_freq);
		auto random_income_Level = context.next_empirical_discrete(income_levels, income_cdf);
		if (entity.income.value() == 0) {
			entity.income.set_both_values(random_income_Level);
		}
		else {
			entity.income = entity.income.value();

			// To prevent education level getting maximal values?, < ? mine just collapsed
			if (entity.age < 31) {
				entity.income = std::max(entity.income.value(), random_income_Level);
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

	int parse_integer(const std::any& value) {
		if (value.has_value()) {
			if (auto* i = std::any_cast<int>(&value)) {
				return *i;
			}
			else if (auto* s = std::any_cast<short>(&value)) {
				return *s;
			}
			else if (auto* f = std::any_cast<float>(&value)) {
				return static_cast<int>(*f);
			}
			else if (auto* d = std::any_cast<double>(&value)) {
				return static_cast<int>(*d);
			}
			else if (auto* s = std::any_cast<std::string>(&value)) {
				return std::stoi(*s);
			}
		}

		return -1;
	}

	float parse_float(const std::any& value)
	{
		if (value.has_value()) {
			// Using pointer to avoid exception
			if (auto* f = std::any_cast<float>(&value)) {
				return *f;
			}
			else if (auto* d = std::any_cast<double>(&value)) {
				return static_cast<float>(*d); // down-cast
			}
			else if (auto* i = std::any_cast<int>(&value)) {
				return static_cast<float>(*i);
			}
			else if (auto* s = std::any_cast<std::string>(&value)) {
				return std::stof(*s);
			}
		}

		return std::nanf("");
	}

	std::unique_ptr<SESModule> hgps::build_ses_module(core::Datastore& manager, ModelInput& config) {

		// SES required data, assuming it has been validated.
		auto& table = config.data();

		auto& gender_col = table.column(config.ses_mapping().entries["gender"]);
		auto& age_col = table.column(config.ses_mapping().entries["age"]);
		auto& edu_col = table.column(config.ses_mapping().entries["education"]);
		auto& inc_col = table.column(config.ses_mapping().entries["income"]);

		std::vector<SESRecord> data;
		for (size_t row = 0; row < table.num_rows(); row++)
		{
			auto gender = parse_gender(gender_col->value(row));
			auto age = parse_integer(age_col->value(row));
			auto edu_value = parse_float(edu_col->value(row));
			float income = parse_float(inc_col->value(row));
			auto weight = 1.0f;

			data.emplace_back(SESRecord(gender, age, edu_value, income, weight));
		}

		return std::make_unique<SESModule>(std::move(data), config.settings().age_range());
	}
}