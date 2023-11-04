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
    auto builder = hc::IntegerDataTableColumnBuilder(name);
    for (const auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stoi(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

hc::FloatDataTableColumnBuilder parse_float_column(const std::string &name,
                                                   const std::vector<std::string> &data) {
    auto builder = hc::FloatDataTableColumnBuilder(name);
    for (const auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stof(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

hc::DoubleDataTableColumnBuilder parse_double_column(const std::string &name,
                                                     const std::vector<std::string> &data) {
    auto builder = hc::DoubleDataTableColumnBuilder(name);
    for (const auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stod(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

} // namespace

namespace host {

hgps::core::DataTable load_datatable_from_csv(const poco::FileInfo &file_info) {
    MEASURE_FUNCTION();
    using namespace rapidcsv;

    bool success = true;
    Document doc{file_info.name.string(), LabelParams{},
                 SeparatorParams{file_info.delimiter.front()}};

    // Validate columns and create file columns map
    auto headers = doc.GetColumnNames();
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
            fmt::print(fg(fmt::color::red), "Error passing column: {}, cause: {}\n", col_name,
                       ex.what());
            success = false;
        }
    }

    if (!success) {
        throw std::runtime_error("Error parsing dataset.");
    }

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
    for (size_t col = 1; col < column_count; col++) {
        data.emplace(hc::to_lower(column_names.at(col)), std::vector<double>{});
    }

    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        auto row = doc.GetRow<std::string>(i);
        for (size_t col = 1; col < row.size(); col++) {
            data.at(column_names[col]).emplace_back(std::stod(row[col]));
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

} // namespace host
