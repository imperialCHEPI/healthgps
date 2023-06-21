#pragma once

#include "HealthGPS.Core/api.h"

namespace host {
namespace hc = hgps::core;

namespace detail {
hc::StringDataTableColumnBuilder parse_string_column(std::string name,
                                                     std::vector<std::string> &data);

hc::IntegerDataTableColumnBuilder parse_int_column(std::string name,
                                                   std::vector<std::string> &data);

hc::FloatDataTableColumnBuilder parse_float_column(std::string name,
                                                   std::vector<std::string> &data);

hc::DoubleDataTableColumnBuilder parse_double_column(std::string name,
                                                     std::vector<std::string> &data);

std::map<std::string, std::size_t>
create_fields_index_mapping(const std::vector<std::string> &column_names,
                            const std::vector<std::string> fields);
} // namespace detail

/// @brief Populates a datatable with the input data file contents
///
/// @remark The datatable is used for checking the initial virtual population
/// against the input data only and is not used as part of the simulation algorithm.
/// If an external test can be used, this table could be dropped from the inputs.
///
/// @param[out] out_table The datatable to be populated with the inputs data
/// @param full_filename The input data file full path
/// @param columns The data file columns data type
/// @param delimiter The data file's columns delimiter character
/// @return true for success, otherwise false.
bool load_datatable_from_csv(hc::DataTable &out_table, std::string full_filename,
                             std::map<std::string, std::string> columns,
                             std::string delimiter = ",");

/// @brief Loads the contents of baseline adjustments file into a table
/// @param full_filename The baseline adjustment file full path
/// @param delimiter The data file's columns delimiter character
/// @return The fully populated table
std::map<hc::Identifier, std::vector<double>>
load_baseline_from_csv(const std::string &full_filename, const std::string delimiter = ",");
} // namespace host