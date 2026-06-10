#include "income_category_layout.h"

#include "exception.h"

#include <fmt/core.h>

namespace hgps::core {

namespace {

IncomeCategoryLayout make_layout(std::size_t count, std::vector<Income> strata,
                                 std::vector<std::string> labels) {
    return IncomeCategoryLayout{
        .count = count, .strata = std::move(strata), .labels = std::move(labels)};
}

} // namespace

IncomeCategoryLayout income_category_layout_from_config(std::string_view categories) {
    if (categories == "3") {
        return make_layout(3u, {Income::low, Income::middle, Income::high},
                           {"Low", "Middle", "High"});
    }
    if (categories == "4") {
        return make_layout(4u,
                           {Income::low, Income::lowermiddle, Income::uppermiddle, Income::high},
                           {"Low", "LowerMid", "UpperMid", "High"});
    }
    if (categories == "5") {
        return make_layout(
            5u,
            {Income::low, Income::lowermiddle, Income::middle, Income::uppermiddle, Income::high},
            {"Low", "LowerMid", "Middle", "UpperMid", "High"});
    }
    throw HgpsException{
        fmt::format(R"(project_requirements.income.categories must be "3", "4", or "5". Got: "{}")",
                    categories)};
}

std::size_t income_table_index(Income income, const IncomeCategoryLayout &layout) {
    for (std::size_t i = 0; i < layout.strata.size(); ++i) {
        if (layout.strata[i] == income) {
            return i;
        }
    }
    throw HgpsException{fmt::format(
        "Income category is not part of the configured {}-category layout", layout.count)};
}

Income income_from_equal_split_bucket(std::size_t bucket, const IncomeCategoryLayout &layout) {
    if (bucket >= layout.strata.size()) {
        return layout.strata.back();
    }
    return layout.strata[bucket];
}

double income_category_numeric(Income income, const IncomeCategoryLayout &layout) {
    const std::size_t idx = income_table_index(income, layout);
    if (layout.count == 3u) {
        switch (income) {
        case Income::low:
            return 1.0;
        case Income::middle:
            return 2.0;
        case Income::high:
            return 4.0;
        default:
            break;
        }
    }
    if (layout.count == 4u) {
        switch (income) {
        case Income::low:
            return 1.0;
        case Income::lowermiddle:
            return 2.0;
        case Income::uppermiddle:
            return 3.0;
        case Income::high:
            return 4.0;
        default:
            break;
        }
    }
    // 5-category: one-to-one 1..5 by table order.
    return static_cast<double>(idx + 1u);
}

} // namespace hgps::core
