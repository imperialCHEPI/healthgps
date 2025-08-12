#include "csvparser.h"
#include <rapidcsv.h>

#include "HealthGPS.Core/scoped_timer.h"
#include "HealthGPS.Core/string_util.h"
#include "HealthGPS/gender_value.h"

#include <fmt/color.h>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace {

namespace hc = hgps::core;

hc::StringDataTableColumnBuilder parse_string_column(const std::string &name,
                                                     const std::vector<std::string> &data) {
    auto builder = hc::StringDataTableColumnBuilder(name);
    for (const auto &value : data) {
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

hc::IntegerDataTableColumnBuilder parse_int_column(const std::string &name,
                                                   const std::vector<std::string> &data) {
    std::cout << "[DEBUG] parse_int_column: Parsing column '" << name << "' with " << data.size()
              << " rows" << std::endl;
    auto builder = hc::IntegerDataTableColumnBuilder(name);
    for (size_t i = 0; i < data.size(); i++) {
        const auto &value = data[i];
        if (value.length() > 0) {
            try {
                int parsed_value = std::stoi(value);
                builder.append(parsed_value);
                if (i % 10000 == 0) {
                    std::cout << "[DEBUG] parse_int_column: Progress - row " << i << "/"
                              << data.size() << std::endl;
                }
            } catch (const std::exception &e) {
                std::cout << "\n[DEBUG] ===== CRASH DETECTED IN INTEGER COLUMN =====" << std::endl;
                std::cout << "[DEBUG] parse_int_column: CRASH at row " << i << " in column '"
                          << name << "'" << std::endl;
                std::cout << "[DEBUG] parse_int_column: Failed value: '" << value << "'"
                          << std::endl;
                std::cout << "[DEBUG] parse_int_column: Error: " << e.what() << std::endl;
                std::cout << "[DEBUG] parse_int_column: Exception type: " << typeid(e).name()
                          << std::endl;

                // Show a few rows before and after the crash for context
                std::cout << "[DEBUG] Context - Previous successful values:" << std::endl;
                for (int j = std::max(0, (int)i - 3); j < i; j++) {
                    if (j < data.size() && data[j].length() > 0) {
                        try {
                            int prev_value = std::stoi(data[j]);
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> "
                                      << prev_value << std::endl;
                        } catch (...) {
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> FAILED"
                                      << std::endl;
                        }
                    }
                }

                std::cout << "[DEBUG] Context - Next few values:" << std::endl;
                for (int j = i; j < std::min((int)data.size(), (int)i + 3); j++) {
                    std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "'" << std::endl;
                }
                std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

                // Create a more specific error message
                std::string specific_error = "CSV parsing failed: Cannot convert value '" + value +
                                             "' to integer in column '" + name + "' at row " +
                                             std::to_string(i) + ". Error: " + e.what();
                throw std::runtime_error(specific_error);
            }
            continue;
        }
        builder.append_null();
        if (i % 1000 == 0) {
            std::cout << "[DEBUG] parse_int_column: Row " << i << " is empty, appended null"
                      << std::endl;
        }
    }
    std::cout << "[DEBUG] parse_int_column: Successfully completed parsing column '" << name << "'"
              << std::endl;
    return builder;
}

hc::FloatDataTableColumnBuilder parse_float_column(const std::string &name,
                                                   const std::vector<std::string> &data) {
    std::cout << "[DEBUG] parse_float_column: Parsing column '" << name << "' with " << data.size()
              << " rows" << std::endl;
    auto builder = hc::FloatDataTableColumnBuilder(name);
    for (size_t i = 0; i < data.size(); i++) {
        const auto &value = data[i];
        if (value.length() > 0) {
            try {
                float parsed_value = std::stof(value);
                builder.append(parsed_value);
                if (i % 10000 == 0) {
                    std::cout << "[DEBUG] parse_float_column: Progress - row " << i << "/"
                              << data.size() << std::endl;
                }
            } catch (const std::exception &e) {
                std::cout << "\n[DEBUG] ===== CRASH DETECTED IN FLOAT COLUMN =====" << std::endl;
                std::cout << "[DEBUG] parse_float_column: CRASH at row " << i << " in column '"
                          << name << "'" << std::endl;
                std::cout << "[DEBUG] parse_float_column: Failed value: '" << value << "'"
                          << std::endl;
                std::cout << "[DEBUG] parse_float_column: Error: " << e.what() << std::endl;
                std::cout << "[DEBUG] parse_float_column: Exception type: " << typeid(e).name()
                          << std::endl;

                // Show a few rows before and after the crash for context
                std::cout << "[DEBUG] Context - Previous successful values:" << std::endl;
                for (int j = std::max(0, (int)i - 3); j < i; j++) {
                    if (j < data.size() && data[j].length() > 0) {
                        try {
                            float prev_value = std::stof(data[j]);
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> "
                                      << prev_value << std::endl;
                        } catch (...) {
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> FAILED"
                                      << std::endl;
                        }
                    }
                }

                std::cout << "[DEBUG] Context - Next few values:" << std::endl;
                for (int j = i; j < std::min((int)data.size(), (int)i + 3); j++) {
                    std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "'" << std::endl;
                }
                std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

                // Create a more specific error message
                std::string specific_error = "CSV parsing failed: Cannot convert value '" + value +
                                             "' to float in column '" + name + "' at row " +
                                             std::to_string(i) + ". Error: " + e.what();
                throw std::runtime_error(specific_error);
            }
            continue;
        }
        builder.append_null();
        if (i % 1000 == 0) {
            std::cout << "[DEBUG] parse_float_column: Row " << i << " is empty, appended null"
                      << std::endl;
        }
    }
    std::cout << "[DEBUG] parse_float_column: Successfully completed parsing column '" << name
              << "'" << std::endl;
    return builder;
}

hc::DoubleDataTableColumnBuilder parse_double_column(const std::string &name,
                                                     const std::vector<std::string> &data) {
    std::cout << "[DEBUG] parse_double_column: Parsing column '" << name << "' with " << data.size()
              << " rows" << std::endl;
    auto builder = hc::DoubleDataTableColumnBuilder(name);
    for (size_t i = 0; i < data.size(); i++) {
        const auto &value = data[i];

        if (value.length() > 0) {
            try {
                double parsed_value = std::stod(value);
                builder.append(parsed_value);
                // Only print every 1000th row to avoid flooding the screen
                if (i % 10000 == 0) {
                    std::cout << "[DEBUG] parse_double_column: Progress - row " << i << "/"
                              << data.size() << std::endl;
                }
            } catch (const std::exception &e) {
                std::cout << "\n[DEBUG] ===== CRASH DETECTED =====" << std::endl;
                std::cout << "[DEBUG] parse_double_column: CRASH at row " << i << " in column '"
                          << name << "'" << std::endl;
                std::cout << "[DEBUG] parse_double_column: Failed value: '" << value << "'"
                          << std::endl;
                std::cout << "[DEBUG] parse_double_column: Error: " << e.what() << std::endl;
                std::cout << "[DEBUG] parse_double_column: Exception type: " << typeid(e).name()
                          << std::endl;

                // Show a few rows before and after the crash for context
                std::cout << "[DEBUG] Context - Previous successful values:" << std::endl;
                for (int j = std::max(0, (int)i - 3); j < i; j++) {
                    if (j < data.size() && data[j].length() > 0) {
                        try {
                            double prev_value = std::stod(data[j]);
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> "
                                      << prev_value << std::endl;
                        } catch (...) {
                            std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "' -> FAILED"
                                      << std::endl;
                        }
                    }
                }

                std::cout << "[DEBUG] Context - Next few values:" << std::endl;
                for (int j = i; j < std::min((int)data.size(), (int)i + 3); j++) {
                    std::cout << "[DEBUG]   Row " << j << ": '" << data[j] << "'" << std::endl;
                }
                std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

                // Create a more specific error message
                std::string specific_error = "CSV parsing failed: Cannot convert value '" + value +
                                             "' to double in column '" + name + "' at row " +
                                             std::to_string(i) + ". Error: " + e.what();
                throw std::runtime_error(specific_error);
            }
            continue;
        }

        builder.append_null();
        // Only print empty rows occasionally
        if (i % 1000 == 0) {
            std::cout << "[DEBUG] parse_double_column: Row " << i << " is empty, appended null"
                      << std::endl;
        }
    }

    std::cout << "[DEBUG] parse_double_column: Successfully completed parsing column '" << name
              << "'" << std::endl;
    return builder;
}

} // namespace

namespace hgps::input {

hgps::core::DataTable load_datatable_from_csv(const FileInfo &file_info) {
    MEASURE_FUNCTION();
    using namespace rapidcsv;

    std::cout << "\n[DEBUG] ===== Starting load_datatable_from_csv =====" << std::endl;
    std::cout << "[DEBUG] File path: " << file_info.name.string() << std::endl;
    std::cout << "[DEBUG] File format: " << file_info.format << std::endl;
    std::cout << "[DEBUG] File delimiter: '" << file_info.delimiter << "'" << std::endl;
    std::cout << "[DEBUG] Number of columns defined: " << file_info.columns.size() << std::endl;

    bool success = true;
    Document doc{file_info.name.string(), LabelParams{},
                 SeparatorParams{file_info.delimiter.front()}};

    // Validate columns and create file columns map
    auto headers = doc.GetColumnNames();
    std::cout << "[DEBUG] CSV headers found: ";
    for (const auto &header : headers) {
        std::cout << "'" << header << "' ";
    }
    std::cout << std::endl;

    std::map<std::string, std::string, hc::case_insensitive::comparator> csv_column_map;
    for (const auto &pair : file_info.columns) {
        // HACK: replace pair with structured bindings once clang allows it.
        const std::string &col_name = pair.first;

        auto is_match = [&col_name](const auto &csv_col_name) {
            return hc::case_insensitive::equals(col_name, csv_col_name);
        };

        auto it = std::find_if(headers.begin(), headers.end(), is_match);
        if (it != headers.end()) {
            csv_column_map[col_name] = *it;
            std::cout << "[DEBUG] Column '" << col_name << "' mapped to CSV column '" << *it << "'"
                      << std::endl;
        } else {
            success = false;
            fmt::print(fg(fmt::color::dark_salmon), "Column: {} not found in dataset.\n", col_name);
        }
    }

    if (!success) {
        throw std::runtime_error("Required columns not found in dataset.");
    }

    hgps::core::DataTable out_table;
    for (const auto &[col_name, csv_col_name] : csv_column_map) {
        std::string col_type = hc::to_lower(file_info.columns.at(col_name));
        std::cout << "[DEBUG] Processing column '" << col_name << "' (type: " << col_type
                  << ") from CSV column '" << csv_col_name << "'" << std::endl;

        // Get raw data
        auto data = doc.GetColumn<std::string>(csv_col_name);

        try {
            if (col_type == "integer") {
                out_table.add(parse_int_column(col_name, data).build());
            } else if (col_type == "double") {
                out_table.add(parse_double_column(col_name, data).build());
            } else if (col_type == "float") {
                out_table.add(parse_float_column(col_name, data).build());
            } else if (col_type == "string") {
                out_table.add(parse_string_column(col_name, data).build());
            } else {
                fmt::print(fg(fmt::color::dark_salmon), "Unknown data type: {} in column: {}\n",
                           col_type, col_name);
                success = false;
            }
        } catch (const std::exception &ex) {
            std::cout << "[DEBUG] ERROR parsing column '" << col_name << "': " << ex.what()
                      << std::endl;
            fmt::print(fg(fmt::color::red), "Error passing column: {}, cause: {}\n", col_name,
                       ex.what());
            success = false;
        }
    }

    if (!success) {
        std::string error_msg = "Error parsing CSV file: " + file_info.name.string() +
                                ". Check the debug output above for specific details.";
        throw std::runtime_error(error_msg);
    }

    std::cout << "[DEBUG] ===== Finished load_datatable_from_csv successfully =====" << std::endl;
    return out_table;
}

std::unordered_map<hgps::core::Identifier, std::vector<double>>
load_baseline_from_csv(const std::string &filename, const std::string &delimiter) {
    using namespace rapidcsv;

    auto data = std::unordered_map<std::string, std::vector<double>>{};
    auto doc = Document{filename, LabelParams{}, SeparatorParams(delimiter.front())};
    auto column_names = doc.GetColumnNames();
    auto column_count = column_names.size();
    if (column_count < 2) {
        throw std::invalid_argument(fmt::format(
            "Invalid number of columns: {} in adjustment file: {}", column_names.size(), filename));
    }

    if (!hc::case_insensitive::equals("age", column_names.at(0))) {
        throw std::invalid_argument(
            fmt::format("Invalid adjustment file format, first column must be age: {}", filename));
    }

    std::transform(column_names.begin(), column_names.end(), column_names.begin(), hc::to_lower);

    // Only process columns that can potentially contain numeric data
    // Skip text columns like 'sex', 'gender', etc.
    for (size_t col = 1; col < column_count; col++) {
        std::string col_name = hc::to_lower(column_names.at(col));

        // Skip known text columns
        if (col_name == "sex" || col_name == "category" || col_name == "group" ||
            col_name == "type" || col_name == "name") {
            std::cout << "[DEBUG] Skipping text column: '" << column_names.at(col) << "'"
                      << std::endl;
            continue;
        }

        data.emplace(col_name, std::vector<double>{});
    }

    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        for (size_t col = 1; col < row.size(); col++) {
            std::string col_name = hc::to_lower(column_names[col]);

            // Skip text columns
            if (col_name == "sex" || col_name == "category" || col_name == "group" ||
                col_name == "type" || col_name == "name") {
                continue;
            }

            // Only process columns that exist in our data map
            if (data.find(col_name) == data.end()) {
                continue;
            }

            try {
                data.at(col_name).emplace_back(std::stod(row[col]));
            } catch (const std::exception &e) {
                std::cout << "\n[DEBUG] ===== CRASH DETECTED IN BASELINE LOADING ====="
                          << std::endl;
                std::cout << "[DEBUG] Function: load_baseline_from_csv" << std::endl;
                std::cout << "[DEBUG] File: " << filename << std::endl;
                std::cout << "[DEBUG] Row: " << i << std::endl;
                std::cout << "[DEBUG] Column: " << col << " (name: '" << column_names[col] << "')"
                          << std::endl;
                std::cout << "[DEBUG] Failed value: '" << row[col] << "'" << std::endl;
                std::cout << "[DEBUG] Error: " << e.what() << std::endl;
                std::cout << "[DEBUG] Exception type: " << typeid(e).name() << std::endl;
                std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

                std::string detailed_error =
                    "Failed to load baseline data from file: " + filename +
                    ". Cannot convert value '" + row[col] + "' to double in column '" +
                    column_names[col] + "' at row " + std::to_string(i) + ". Error: " + e.what();
                throw std::runtime_error(detailed_error);
            }
        }
    }

    for (auto &col : data) {
        col.second.shrink_to_fit();
    }

    auto result = std::unordered_map<hc::Identifier, std::vector<double>>{};
    for (const auto &col : data) {
        result.emplace(hc::Identifier{col.first}, col.second);
    }

    return result;
}

} // namespace hgps::input
