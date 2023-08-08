#pragma once

#include "HealthGPS.Core/api.h"

namespace host {

/// @brief Populates a datatable with the input data file contents
///
/// @remark The datatable is used for checking the initial virtual population
/// against the input data only and is not used as part of the simulation algorithm.
/// If an external test can be used, this table could be dropped from the inputs.
///
/// @param[out] out_table The datatable to be populated with the inputs data
/// @param filename The input data file full path
/// @param columns The data file columns data type
/// @param delimiter The data file's columns delimiter character
/// @return true for success, otherwise false.
bool load_datatable_from_csv(hgps::core::DataTable &out_table, const std::string &filename,
                             const std::map<std::string, std::string> &columns,
                             const std::string &delimiter = ",");

/// @brief Loads the contents of baseline adjustments file into a table
/// @param filename The baseline adjustment file full path
/// @param delimiter The data file's columns delimiter character
/// @return The fully populated table
std::map<hgps::core::Identifier, std::vector<double>>
load_baseline_from_csv(const std::string &filename, const std::string delimiter = ",");

} // namespace host
