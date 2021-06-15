#pragma once

#include <rapidcsv.h>
#include "HealthGPS.Datastore/api.h"
#include "utility.h"

namespace hd = hgps::data;

auto get_string_column(std::string name, std::vector<std::string>& data)
{
	auto builder = hd::StringDataTableColumnBuilder(name);
	for (auto& value : data) {
		// trim string 
		auto str = trim(value);

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
	auto builder = hd::IntegerDataTableColumnBuilder(name);
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
	auto builder = hd::FloatDataTableColumnBuilder(name);
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
	auto builder = hd::DoubleDataTableColumnBuilder(name);
	for (auto& value : data) {
		if (value.length() > 0) {
			builder.append(std::stod(value));
			continue;
		}

		builder.append_null();
	}

	return builder;
}

bool load_csv(
	const std::string& full_filename,
	const std::unordered_map<std::string, std::string> columns,
	hd::DataTable& out_table,
	const std::string delimiter = ",") {

	using namespace rapidcsv;

	Document doc(full_filename, LabelParams(0, 0), SeparatorParams(delimiter.front()));

	// Validate columns and create file columns map
	auto mismatch = 0;
	auto headers = doc.GetColumnNames();
	std::unordered_map<std::string, std::string> csv_cols;
	for (auto& pair : columns)
	{
		auto is_match = [&pair](const auto& str) {return iequals(pair.first, str); };
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
		auto col_type = to_lower(columns.at(pair.first));

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
