#include "pch.h"

#include "HealthGPS.Input/configuration_parsing_helpers.h"
#include "HealthGPS.Input/poco.h"

TEST(ConfigLegacyFields, RejectsDeprecatedRootFieldsWhenProjectRequirementsPresent) {
    using namespace hgps::input;

    const nlohmann::json j = {{"project_requirements", nlohmann::json::object()},
                              {"trend_type", "null"}};
    EXPECT_THROW(reject_deprecated_root_config_fields(j), ConfigurationError);
}

TEST(ConfigLegacyFields, MapsLegacyTrendTypeIntoProjectRequirements) {
    using namespace hgps::input;

    ProjectRequirements req{};
    const nlohmann::json j = {{"trend_type", "income_trend"}};
    apply_legacy_root_config_fields(j, req);
    EXPECT_TRUE(req.trend.enabled);
    EXPECT_EQ("income_trend", req.trend.type);
}

TEST(ConfigLegacyFields, MapsLegacyIncomeCategoriesIntoProjectRequirements) {
    using namespace hgps::input;

    ProjectRequirements req{};
    const nlohmann::json j = {{"income_categories", "5"}};
    apply_legacy_root_config_fields(j, req);
    EXPECT_EQ("5", req.income.categories);
}
