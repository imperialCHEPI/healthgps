#pragma once

#include <rapidcsv.h>
#include "HealthGPS.Core/api.h"
#include "HealthGPS/gender_value.h"
#include "HealthGPS.Core/string_util.h"

namespace hc = hgps::core;

auto get_string_column(std::string name, std::vector<std::string>& data)
{
	auto builder = hc::StringDataTableColumnBuilder(name);
	for (auto& value : data) {
		// trim string 
		auto str = hc::trim(value);

		if (str.length() > 0) {
			builder.append(str);
			continue;
		}

		builder.append_null();
	}

	return builder;
}

auto get_int_column(std::string name, std::vector<std::string>& data)
{
	auto builder = hc::IntegerDataTableColumnBuilder(name);
	for (auto& value : data) {
		if (value.length() > 0) {
			builder.append(std::stoi(value));
			continue;
		}

		builder.append_null();
	}

	return builder;
}

auto get_float_column(std::string name, std::vector<std::string>& data)
{
	auto builder = hc::FloatDataTableColumnBuilder(name);
	for (auto& value : data) {
		if (value.length() > 0) {
			builder.append(std::stof(value));
			continue;
		}

		builder.append_null();
	}

	return builder;
}

auto get_double_column(std::string name, std::vector<std::string>& data)
{
	auto builder = hc::DoubleDataTableColumnBuilder(name);
	for (auto& value : data) {
		if (value.length() > 0) {
			builder.append(std::stod(value));
			continue;
		}

		builder.append_null();
	}

	return builder;
}

bool load_datatable_csv(
	const std::string& full_filename,
	const std::map<std::string, std::string> columns,
	hc::DataTable& out_table,
	const std::string delimiter = ",") {

	using namespace rapidcsv;

	Document doc(full_filename, LabelParams(0, 0), SeparatorParams(delimiter.front()));

	// Validate columns and create file columns map
	auto mismatch = 0;
	auto headers = doc.GetColumnNames();
	std::map<std::string, std::string, hc::case_insensitive::comparator> csv_cols;
	for (auto& pair : columns)
	{
		auto is_match = [&pair](const auto& str) {
			return hc::case_insensitive::equals(pair.first, str);
		};

		if (auto it = std::find_if(headers.begin(), headers.end(), is_match); it != headers.end()) {
			csv_cols[pair.first] = *it;
		}
		else {
			mismatch++;
			std::cout << "Column: " << pair.first << " not found." << std::endl;
		}
	}

	if (mismatch > 0) {
		return false;
	}

	for (auto& pair : csv_cols)
	{
		auto col_type = hc::to_lower(columns.at(pair.first));

		// Get raw data
		auto data = doc.GetColumn<std::string>(pair.second);
		try
		{
			if (col_type == "integer") {
				out_table.add(get_int_column(pair.first, data).build());
			}
			else if (col_type == "double") {
				out_table.add(get_double_column(pair.first, data).build());
			}
			else if (col_type == "float") {
				out_table.add(get_float_column(pair.first, data).build());
			}
			else if (col_type == "string") {
				out_table.add(get_string_column(pair.first, data).build());
			}
			else {
				std::cout << std::format("Invalid data type: {} in column: {}\n", col_type, pair.first);
			}
		}
		catch (const std::exception& ex)
		{
			std::cout << std::format("Error passing column: {}, cause: {}\n", pair.first, ex.what());
		}
	}

	return true;
}

std::map<std::string, std::size_t> create_fields_index_mapping(
	const std::vector<std::string>& column_names, const std::vector<std::string> fields)
{
	auto mapping = std::map<std::string, std::size_t>();
	for (auto& field : fields) {
		auto field_index = hc::case_insensitive::index_of(column_names, field);
		if (field_index < 0) {
			throw std::out_of_range(std::format("Required field {} not found.", field));
		}

		mapping.emplace(field, field_index);
	}

	return mapping;
}

std::map<int, std::map<std::string,hgps::DoubleGenderValue>> load_baseline_csv(
	const hc::IntegerInterval& time_range, const std::string& full_filename, const std::string delimiter = ",")
{
	using namespace hgps;
	using namespace rapidcsv;

	auto data = std::map<int, std::map<std::string, DoubleGenderValue>>{};

	auto doc = Document{ full_filename, LabelParams{}, SeparatorParams(delimiter.front()) };
	auto mapping = create_fields_index_mapping(doc.GetColumnNames(),
		{ "Time", "RiskFactor", "Male", "Female" });

	for (size_t i = 0; i < doc.GetRowCount(); i++) {
		auto row = doc.GetRow<std::string>(i);
		auto time = std::stoi(row[mapping["Time"]]);
		if (time < time_range.lower() || time > time_range.upper()) {
			continue;
		}

		auto factor = hc::to_lower(row[mapping["RiskFactor"]]);
		data[time].emplace(factor, DoubleGenderValue{
				std::stod(row[mapping["Male"]]),
				std::stod(row[mapping["Female"]])
			});
	}

	return data;
}