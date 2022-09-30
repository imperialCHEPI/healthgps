#pragma once

#include "HealthGPS.Core/api.h"

namespace hc = hgps::core;

hc::StringDataTableColumnBuilder get_string_column(std::string name, std::vector<std::string>& data);

hc::IntegerDataTableColumnBuilder get_int_column(std::string name, std::vector<std::string>& data);

hc::FloatDataTableColumnBuilder get_float_column(std::string name, std::vector<std::string>& data);

hc::DoubleDataTableColumnBuilder get_double_column(std::string name, std::vector<std::string>& data);

bool load_datatable_csv(hc::DataTable& out_table, std::string full_filename,
	std::map<std::string,std::string> columns, std::string delimiter = ",");

std::map<std::string, std::size_t> create_fields_index_mapping(
	const std::vector<std::string>& column_names, const std::vector<std::string> fields);

std::map<std::string, std::vector<double>> load_baseline_csv(
	const std::string& full_filename, const std::string delimiter = ",");