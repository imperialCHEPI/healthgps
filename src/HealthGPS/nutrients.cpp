#include "nutrients.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Core/string_util.h"

#include "fmt/color.h"
#include "fmt/format.h"

#include <algorithm>
#include <iterator>

namespace hgps {
namespace detail {
std::vector<size_t> get_nutrient_indexes(const std::vector<std::string> &column_names,
                                         const std::vector<core::Identifier> &nutrients) {
    bool nutrients_missing = false;

    std::vector<size_t> nutrient_idx; // Which column each nutrient is in
    nutrient_idx.reserve(nutrients.size());
    for (const auto &nutrient : nutrients) {
        const auto it = std::find(column_names.begin(), column_names.end(), nutrient);
        if (it == column_names.end()) {
            if (!nutrients_missing) {
                nutrients_missing = true;
                fmt::print(fmt::fg(fmt::color::yellow), "The following nutrients were missing:\n");
            }

            fmt::print(fmt::fg(fmt::color::yellow), "- {}\n", nutrient.to_string());
        } else {
            nutrient_idx.push_back(std::distance(column_names.begin(), it));
        }
    }

    if (nutrients_missing) {
        throw core::HgpsException{"One or more nutrients were missing from CSV file"};
    }

    return nutrient_idx;
}

std::unordered_map<core::Identifier, std::vector<double>>
get_nutrient_table(rapidcsv::Document &doc, const std::vector<core::Identifier> &nutrients,
                   const std::vector<size_t> &nutrient_idx) {

    std::unordered_map<core::Identifier, std::vector<double>> out;
    out.reserve(doc.GetRowCount());
    for (size_t i = 0; i < doc.GetRowCount(); i++) {
        std::vector<double> row;
        row.reserve(nutrients.size());
        for (size_t j = 0; j < nutrients.size(); j++) {
            row.push_back(doc.GetCell<double>(nutrient_idx[j], i));
        }

        out.emplace(doc.GetRowName(i), std::move(row));
    }

    return out;
}
} // namespace detail

std::unordered_map<core::Identifier, std::vector<double>>
load_nutrient_table(const std::string &csv_path, const std::vector<core::Identifier> &nutrients) {
    rapidcsv::Document doc{csv_path, rapidcsv::LabelParams{0, 0}};
    const auto column_names = doc.GetColumnNames();

    const auto nutrient_idx = detail::get_nutrient_indexes(column_names, nutrients);
    return detail::get_nutrient_table(doc, nutrients, nutrient_idx);
}
} // namespace hgps
