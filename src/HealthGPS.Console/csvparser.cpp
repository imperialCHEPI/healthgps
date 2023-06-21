#include "csvparser.h"
#include <rapidcsv.h>

#include "HealthGPS.Core/scoped_timer.h"
#include "HealthGPS.Core/string_util.h"
#include "HealthGPS/gender_value.h"
#include "HealthGPS/hierarchical_model_types.h"

#include <fmt/color.h>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                                         \
    hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

namespace host {
namespace detail {
hc::StringDataTableColumnBuilder parse_string_column(std::string name,
                                                     std::vector<std::string> &data) {
    auto builder = hc::StringDataTableColumnBuilder(name);
    for (auto &value : data) {
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

hc::IntegerDataTableColumnBuilder parse_int_column(std::string name,
                                                   std::vector<std::string> &data) {
    auto builder = hc::IntegerDataTableColumnBuilder(name);
    for (auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stoi(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

hc::FloatDataTableColumnBuilder parse_float_column(std::string name,
                                                   std::vector<std::string> &data) {
    auto builder = hc::FloatDataTableColumnBuilder(name);
    for (auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stof(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

hc::DoubleDataTableColumnBuilder parse_double_column(std::string name,
                                                     std::vector<std::string> &data) {
    auto builder = hc::DoubleDataTableColumnBuilder(name);
    for (auto &value : data) {
        if (value.length() > 0) {
            builder.append(std::stod(value));
            continue;
        }

        builder.append_null();
    }

    return builder;
}

std::map<std::string, std::size_t>
create_fields_index_mapping(const std::vector<std::string> &column_names,
                            const std::vector<std::string> fields) {
    auto mapping = std::map<std::string, std::size_t>();
    for (auto &field : fields) {
        auto field_index = hc::case_insensitive::index_of(column_names, field);
        if (field_index < 0) {
            throw std::out_of_range(fmt::format("Required field {} not found.", field));
        }

        mapping.emplace(field, field_index);
    }

    return mapping;
}
} // namespace detail

bool load_datatable_from_csv(hc::DataTable &out_table, std::string full_filename,
                             std::map<std::string, std::string> columns, std::string delimiter) {
    MEASURE_FUNCTION();
    using namespace rapidcsv;

    Document doc(full_filename, LabelParams(0, 0), SeparatorParams(delimiter.front()));

    // Validate columns and create file columns map
    auto mismatch = 0;
    auto headers = doc.GetColumnNames();
    std::map<std::string, std::string, hc::case_insensitive::comparator> csv_cols;
    for (auto &pair : columns) {
        auto is_match = [&pair](const auto &str) {
            return hc::case_insensitive::equals(pair.first, str);
        };

        if (auto it = std::find_if(headers.begin(), headers.end(), is_match); it != headers.end()) {
            csv_cols[pair.first] = *it;
        } else {
            mismatch++;
            fmt::print(fg(fmt::color::dark_salmon), "Column: {} not found in dataset.\n",
                       pair.first);
        }
    }

    if (mismatch > 0) {
        return false;
    }

    auto result = true;
    for (auto &pair : csv_cols) {
        auto col_type = hc::to_lower(columns.at(pair.first));

        // Get raw data
        auto data = doc.GetColumn<std::string>(pair.second);
        try {
            if (col_type == "integer") {
                out_table.add(detail::parse_int_column(pair.first, data).build());
            } else if (col_type == "double") {
                out_table.add(detail::parse_double_column(pair.first, data).build());
            } else if (col_type == "float") {
                out_table.add(detail::parse_float_column(pair.first, data).build());
            } else if (col_type == "string") {
                out_table.add(detail::parse_string_column(pair.first, data).build());
            } else {
                fmt::print(fg(fmt::color::dark_salmon), "Unknown data type: {} in column: {}\n",
                           col_type, pair.first);
                result = false;
            }
        } catch (const std::exception &ex) {
            fmt::print(fg(fmt::color::red), "Error passing column: {}, cause: {}\n", pair.first,
                       ex.what());
            result = false;
        }
    }

    return result;
}

std::map<hc::Identifier, std::vector<double>>
load_baseline_from_csv(const std::string &full_filename, const std::string delimiter) {
    using namespace hgps;
    using namespace rapidcsv;

    auto data = std::map<std::string, std::vector<double>>{};
    auto doc = Document{full_filename, LabelParams{}, SeparatorParams(delimiter.front())};
    auto column_names = doc.GetColumnNames();
    auto column_count = column_names.size();
    if (column_count < 2) {
        throw std::invalid_argument(
            fmt::format("Invalid number of columns: {} in adjustment file: {}", column_names.size(),
                        full_filename));
    }

    if (!hc::case_insensitive::equals("age", column_names.at(0))) {
        throw std::invalid_argument(fmt::format(
            "Invalid adjustment file format, first column must be age: {}", full_filename));
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

    auto result = std::map<hc::Identifier, std::vector<double>>{};
    for (const auto &col : data) {
        result.emplace(hc::Identifier{col.first}, col.second);
    }

    return result;
}
} // namespace host