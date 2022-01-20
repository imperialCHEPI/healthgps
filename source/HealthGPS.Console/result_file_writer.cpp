#include "result_file_writer.h"

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>

ResultFileWriter::ResultFileWriter(const std::filesystem::path file_name, const ModelInfo info)
	: info_{ info } 
{
	stream_.open(file_name, std::ofstream::out | std::ofstream::app);
	if (stream_.fail() || !stream_.is_open()) {
		throw std::invalid_argument(fmt::format("Cannot open output file: {}", file_name.string()));
	}

	write_json_begin();
}

ResultFileWriter::ResultFileWriter(ResultFileWriter&& other) noexcept {
	stream_.close();
	stream_ = std::move(other.stream_);
	info_ = std::move(other.info_);
}

ResultFileWriter& ResultFileWriter::operator=(ResultFileWriter&& other) noexcept {
	stream_.close();
	stream_ = std::move(other.stream_);
	info_ = std::move(other.info_);
	return *this;
}

ResultFileWriter::~ResultFileWriter() {
	if (stream_.is_open()) {
		write_json_end();
		stream_.flush();
		stream_.close();
	}
}

void ResultFileWriter::write(const hgps::ResultEventMessage& message) {
	std::scoped_lock lock(lock_mutex_);
	if (first_row_.load()) {
		first_row_ = false;
	}
	else {
		stream_ << ",";
	}

	stream_ << to_json_string(message);
}

void ResultFileWriter::write_json_begin() {
	using json = nlohmann::ordered_json;

	auto tp = std::chrono::system_clock::now();
	json msg = {
		{"experiment", {
			{"model", info_.name},
			{"version", info_.version},
			{"time_of_day", fmt::format("{0:%F %H:%M:}{1:%S} {0:%Z}", tp, tp.time_since_epoch())}
		}},
		{"result",{1,2}} };

	auto json_header = msg.dump();
	auto array_start = json_header.find_last_of("[");
	stream_ << json_header.substr(0, array_start + 1);
}

void ResultFileWriter::write_json_end() {
	stream_ << "]}";
}

std::string ResultFileWriter::to_json_string(const hgps::ResultEventMessage& message) const {
	using json = nlohmann::ordered_json;
	json msg = {
		{"id", message.id()},
		{"source", message.source},
		{"run", message.run_number},
		{"time", message.model_time},
		{"population", {
			{"size", message.content.population_size},
			{"alive", message.content.number_alive},
			{"migrating", message.content.number_emigrated},
			{"dead", message.content.number_dead},
		}},
		{"average_age", {
			{"male",message.content.average_age.male},
			{"female",message.content.average_age.female}
		}},
		{"indicators", {
			{"YLL", message.content.indicators.years_of_life_lost},
			{"YLD", message.content.indicators.years_lived_with_disability},
			{"DALY", message.content.indicators.disablity_adjusted_life_years},
		}},
	};

	for (const auto& factor : message.content.risk_ractor_average) {
		msg["risk_factors_average"][factor.first] = { 
			{"male", factor.second.male},
			{"female", factor.second.female}
		};
	}

	for (const auto& disease : message.content.disease_prevalence) {
		msg["disease_prevalence"][disease.first] = { 
			{"male", disease.second.male},
			{"female", disease.second.female}
		};
	}

	msg["metrics"] = message.content.metrics;

	return msg.dump();
}
